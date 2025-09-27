const std = @import("std");
const config = @import("config.zig");
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

const frameSize: comptime_int = (1102 * 4);
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
            if (cd.*.buffer) |buf| {
                // TODO convert to json to pass along all info inside capture_data_t
                const converted_buffer: [*]const u8 = @ptrCast(@alignCast(buf));
                if (chebi.message.Message.init_with_body(
                    std.heap.smp_allocator,
                    info.conf.topic,
                    converted_buffer[0..cd.*.buffer_len],
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
            }
            audio.capture_data_destroy(@ptrCast(cd));
        }
    }
}

pub fn main() !void {
    var conf = try config.config(std.heap.smp_allocator);
    defer conf.deinit();

    defer g_info.stop();

    const empty_sig: [16]c_ulong = @splat(0);
    _ = std.c.sigaction(std.c.SIG.INT, &.{
        .handler = .{ .handler = interrupt_stop },
        .mask = empty_sig,
        .flags = 0,
    }, null);

    _ = try std.Thread.spawn(.{
        .allocator = std.heap.smp_allocator,
    }, handle_capture, .{&g_info});
    const addr = try std.net.Address.parseIp4(conf.ip, conf.port);
    var c = try client.Client.init(std.heap.smp_allocator, addr);
    defer c.deinit();
    g_info.c = &c;
    try c.connect();

    const capture_opt = audio.capture_create(@intCast(frameSize));
    const playback_opt = audio.playback_create(@intCast(frameSize));
    if (capture_opt == null or playback_opt == null) {
        return Error.audio_creation_failed;
    }
    if (capture_opt) |cap| {
        g_info.cap = cap;
    }
    if (playback_opt) |play| {
        g_info.play = play;
    }
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
            var data = audio.capture_data_create();
            const cd_buffer: []u8 = try std.heap.c_allocator.dupe(u8, payload);
            data.*.buffer = @ptrCast(cd_buffer.ptr);
            data.*.buffer_len = payload.len;
            data.*.channels = 1;
            data.*.format = audio.ma_format_f32;
            data.*.sizeInFrames = 1102;
            const queue_result: audio.ma_result = audio.playback_queue(g_info.play, data);
            if (queue_result != audio.MA_SUCCESS) {
                std.debug.print("playback_queue failed: code({})\n", .{queue_result});
            }
            audio.capture_data_destroy(&data);
        }
    }
}
