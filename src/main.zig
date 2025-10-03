const std = @import("std");
const config = @import("config.zig");
const capture = @import("capture_data.zig");
const chebi = @import("chebi");
const client = chebi.client;

const rb = @import("rb");
const Ring = rb.RingBuffer(10, capture.CaptureData);

const audio = @cImport({
    @cInclude("audio_capture.h");
    @cInclude("audio_playback.h");
});

var g_alloc = std.heap.smp_allocator;
const Error = error{
    audio_creation_failed,
    playback_start_failed,
    capture_start_failed,
    unknown_format,
    not_supported,
};

const Info = struct {
    ring: *Ring,
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
    .ring = undefined,
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
    @memcpy(result.buffer, @as([*]const u8, @ptrCast(cap.buffer.?)));
    return result;
}

fn cap_data_decode(cap: capture.CaptureData, out: *audio.capture_data_t) void {
    out.sizeInFrames = @intCast(cap.sizeInFrames);
    out.format = @intCast(cap.format);
    out.channels = @intCast(cap.channels);
    out.buffer_len = cap.buffer.len;
    out.buffer = cap.buffer.ptr;
}

fn handle_broadcast(info: *Info) void {
    while (info.running) {
        const items_opt = info.ring.read_when_full(g_alloc, std.time.ns_per_s * 1) catch unreachable;
        if (items_opt) |items| {
            defer g_alloc.free(items);
            for (items) |*cap| {
                defer cap.*.deinit();
                const marshal_data: []const u8 = cap.*.marshal() catch |err| {
                    std.debug.print("failed to marshal capture_data: {any}\n", .{err});
                    continue;
                };
                defer cap.*.alloc.free(marshal_data);
                if (chebi.message.Message.init_with_body(
                    g_alloc,
                    info.conf.topic,
                    marshal_data,
                    .text,
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
        }
    }
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
            continue;
        }
        if (cd_opt) |*cd| {
            defer audio.capture_data_destroy(@ptrCast(cd));
            if (cd.*.buffer) |_| {
                // TODO break up data into smaller packets
                // maybe, or maybe we should handle this in the audio lib
                const cap_data: capture.CaptureData = cap_data_encode(g_alloc, cd.*) catch |err| {
                    std.debug.print("failed to encode capture_data: {any}\n", .{err});
                    continue;
                };
                info.ring.write(cap_data) catch |err| {
                    std.debug.print("ring buffer write error: {any}\n", .{err});
                };
            }
        }
    }
}

fn create_capture() !void {
    const capture_opt = audio.capture_create(200);
    if (capture_opt == null) {
        return Error.audio_creation_failed;
    }
    if (capture_opt) |cap| {
        g_info.cap = cap;
    }
    // capture thread
    _ = try std.Thread.spawn(.{
        .allocator = g_alloc,
    }, handle_capture, .{&g_info});
    // broadcast thread
    _ = try std.Thread.spawn(.{
        .allocator = g_alloc,
    }, handle_broadcast, .{&g_info});
    const result: audio.ma_result = audio.capture_start(g_info.cap);
    if (result != audio.MA_SUCCESS) {
        std.debug.print("capture failed to start: code({})\n", .{result});
        return Error.capture_start_failed;
    }
}

fn create_playback() !void {
    const playback_opt = audio.playback_create(200);
    if (playback_opt == null) {
        return Error.audio_creation_failed;
    }
    if (playback_opt) |play| {
        g_info.play = play;
    }
    const result = audio.playback_start(g_info.play);
    if (result != audio.MA_SUCCESS) {
        std.debug.print("playback failed to start: code({})\n", .{result});
        return Error.playback_start_failed;
    }
    try g_info.c.subscribe(g_info.conf.topic);
}

pub fn main() !void {
    var conf = try config.config(g_alloc);
    defer conf.deinit();

    defer g_info.stop();
    g_info.conf = conf;

    var local_ring: Ring = .init();
    g_info.ring = &local_ring;

    const empty_sig: [16]c_ulong = @splat(0);
    _ = std.c.sigaction(std.c.SIG.INT, &.{
        .handler = .{ .handler = interrupt_stop },
        .mask = empty_sig,
        .flags = 0,
    }, null);

    const addr = try std.net.Address.parseIp4(conf.ip, conf.port);
    var c = try client.Client.init(g_alloc, addr);
    defer c.deinit();
    g_info.c = &c;
    try c.connect();

    if (conf.capture_only) {
        try create_capture();
    }
    if (conf.playback_only) {
        try create_playback();
    }

    while (g_info.running) {
        var msg = try g_info.c.next_msg();
        defer msg.deinit();
        if (conf.capture_only) {
            continue;
        }
        if (msg.payload) |payload| {
            var data: capture.CaptureData = .init(g_alloc);
            defer data.deinit();
            try data.unmarshal(payload);
            var cd: audio.capture_data_t = .{};
            cap_data_decode(data, &cd);
            const queue_result: audio.ma_result = audio.playback_queue(g_info.play, &cd);
            if (queue_result != audio.MA_SUCCESS) {
                std.debug.print("playback_queue failed: code({})\n", .{queue_result});
            }
        }
    }
}
