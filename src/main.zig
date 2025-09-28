const std = @import("std");
const config = @import("config.zig");
const capture = @import("capture_data.zig");
const chebi = @import("chebi");
const client = chebi.client;

const audio = @cImport({
    @cInclude("audio_capture.h");
    @cInclude("audio_playback.h");
});

const Error = error{
    audio_creation_failed,
    playback_start_failed,
    capture_start_failed,
};

const Info = struct {
    running: bool = true,
    cap: *audio.capture_t,
    play: *audio.playback_t,
    c: *client.Client,
    conf: config.Config,

    pub fn stop(self: *Info) void {
        self.running = false;
    }
};

var g_info: Info = .{
    .c = undefined,
    .play = undefined,
    .cap = undefined,
    .conf = undefined,
};

export fn interrupt_stop(_: i32) void {
    g_info.running = false;
}

fn cap_data_encode(alloc: std.mem.Allocator, cap: *audio.capture_data_t) !capture.CaptureData {
    var result: capture.CaptureData = .init(alloc);
    result.sizeInFrames = @intCast(cap.sizeInFrames);
    result.format = @intCast(cap.format);
    result.channels = @intCast(cap.channels);
    result.buffer = try alloc.alloc(u8, cap.buffer_len);
    @memcpy(result.buffer, @as([*]u8, @ptrCast(cap.buffer.?))[0..cap.buffer_len]);
    return result;
}
fn cap_data_decode(alloc: std.mem.Allocator, cap: capture.CaptureData) !*audio.capture_data_t {
    var result: *audio.capture_data_t = audio.capture_data_create();
    result.sizeInFrames = @intCast(cap.sizeInFrames);
    result.format = @intCast(cap.format);
    result.channels = @intCast(cap.channels);
    result.buffer_len = cap.buffer.len;
    const tmp_buffer: []u8 = try alloc.alloc(u8, cap.buffer.len);
    @memcpy(tmp_buffer, cap.buffer);
    result.buffer = @ptrCast(tmp_buffer.ptr);
    return result;
}

fn handle_capture(info: *Info) void {
    while (info.running) {
        var cd_opt: ?*audio.capture_data_t = null;
        const result: audio.ma_result = audio.capture_next_available(info.cap, &cd_opt);
        if (result != audio.MA_SUCCESS and result != audio.MA_NO_DATA_AVAILABLE) {
            std.debug.print("capture_next_available failed: code({})\n", .{result});
            // if we encounter an error, lets wait a time before trying again.
            const wait_info: std.c.timespec = .{
                .sec = 0,
                .nsec = std.time.ns_per_ms * 250,
            };
            _ = std.c.nanosleep(&wait_info, null);
        }
        if (cd_opt) |*cd| {
            if (cd.*.buffer) |_| {
                // TODO convert to json to pass along all info inside capture_data_t
                var cap_data_opt: ?capture.CaptureData = cap_data_encode(std.heap.smp_allocator, cd.*) catch null;
                if (cap_data_opt) |*cap_data| {
                    defer cap_data.*.deinit();
                    const marshal_data_opt: ?[]const u8 = cap_data.marshal() catch null;
                    if (marshal_data_opt) |marshal_data| {
                        defer cap_data.alloc.free(marshal_data);
                        if (chebi.message.Message.init_with_body(
                            std.heap.smp_allocator,
                            info.conf.topic,
                            marshal_data,
                            .bin,
                        )) |msg| {
                            var local_msg: chebi.message.Message = msg;
                            info.c.write_msg(&local_msg) catch |err| {
                                std.debug.print("write capture msg failed: {any}\n", .{err});
                            };
                            local_msg.deinit();
                        } else |err| {
                            std.debug.print("init_with_body failed: {any}\n", .{err});
                        }
                    } else {
                        std.debug.print("failed to marshal capture_data.\n", .{});
                    }
                } else {
                    std.debug.print("failed to encode capture_data.\n", .{});
                }
            }
            audio.capture_data_destroy(@ptrCast(cd));
        }
    }
}

pub fn main() !void {
    var conf = try config.config(std.heap.smp_allocator);
    defer conf.deinit();

    defer g_info.stop();
    g_info.conf = conf;

    const empty_sig: [16]c_ulong = @splat(0);
    _ = std.c.sigaction(std.c.SIG.INT, &.{
        .handler = .{ .handler = interrupt_stop },
        .mask = empty_sig,
        .flags = 0,
    }, null);

    const addr = try std.net.Address.parseIp4(conf.ip, conf.port);
    var c = try client.Client.init(std.heap.smp_allocator, addr);
    defer c.deinit();
    g_info.c = &c;
    try c.connect();

    const capture_opt = audio.capture_create(5);
    const playback_opt = audio.playback_create(5);
    if (capture_opt == null or playback_opt == null) {
        return Error.audio_creation_failed;
    }
    if (capture_opt) |cap| {
        g_info.cap = cap;
    }
    if (playback_opt) |play| {
        g_info.play = play;
    }
    _ = try std.Thread.spawn(.{
        .allocator = std.heap.smp_allocator,
    }, handle_capture, .{&g_info});
    var result: audio.ma_result = audio.capture_start(g_info.cap);
    if (result != audio.MA_SUCCESS) {
        std.debug.print("capture failed to start: code({})\n", .{result});
        return Error.capture_start_failed;
    }
    result = audio.playback_start(g_info.play);
    if (result != audio.MA_SUCCESS) {
        std.debug.print("playback failed to start: code({})\n", .{result});
        return Error.playback_start_failed;
    }

    while (g_info.running) {
        var msg = try g_info.c.next_msg();
        defer msg.deinit();
        if (msg.payload) |payload| {
            // TODO convert to unmarsheling json to get all g_info inside capture_data_t
            var data: capture.CaptureData = .init(std.heap.smp_allocator);
            try data.unmarshal(payload);
            var cd = try cap_data_decode(std.heap.smp_allocator, data);
            const queue_result: audio.ma_result = audio.playback_queue(g_info.play, cd);
            if (queue_result != audio.MA_SUCCESS) {
                std.debug.print("playback_queue failed: code({})\n", .{queue_result});
            }
            audio.capture_data_destroy(@ptrCast(&cd));
        }
    }
}
