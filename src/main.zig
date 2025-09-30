const std = @import("std");
const config = @import("config.zig");
const capture = @import("capture_data.zig");
const chebi = @import("chebi");
const client = chebi.client;

const audio = @cImport({
    @cInclude("audio_capture.h");
    @cInclude("audio_playback.h");
});

var g_alloc = std.heap.DebugAllocator(.{}){};

const Error = error{
    audio_creation_failed,
    playback_start_failed,
    capture_start_failed,
    unknown_format,
    not_supported,
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

fn get_format_size(format: audio.ma_format) !usize {
    return switch (format) {
        audio.ma_format_u8 => @sizeOf(u8),
        audio.ma_format_s16 => @sizeOf(i16),
        audio.ma_format_s24 => @sizeOf(i24),
        audio.ma_format_s32 => @sizeOf(i32),
        audio.ma_format_f32 => @sizeOf(f32),
        else => Error.unknown_format,
    };
}

fn encode_format_data(comptime T: type, comptime C: type, cap: *audio.capture_data_t, result: *capture.CaptureData) void {
    const cap_data: [*c]const C = @ptrCast(@alignCast(cap.buffer.?));
    var index: usize = 0;
    while (index < result.buffer.len) {
        std.mem.writePackedInt(
            T,
            result.buffer[index..],
            0,
            @intCast(cap_data[index]),
            .little,
        );
        index += @sizeOf(T);
    }
}

fn cap_data_encode(alloc: std.mem.Allocator, cap: *audio.capture_data_t) !capture.CaptureData {
    var result: capture.CaptureData = .init(alloc);
    result.sizeInFrames = @intCast(cap.sizeInFrames);
    result.format = @intCast(cap.format);
    result.channels = @intCast(cap.channels);
    result.buffer = try alloc.alloc(u8, cap.buffer_len * try get_format_size(cap.format));
    errdefer alloc.free(result.buffer);
    switch (cap.format) {
        audio.ma_format_u8 => {
            encode_format_data(u8, u8, cap, &result);
        },
        audio.ma_format_s16 => {
            encode_format_data(i16, i16, cap, &result);
        },
        audio.ma_format_s24 => {
            encode_format_data(i24, i32, cap, &result);
        },
        audio.ma_format_s32 => {
            encode_format_data(i32, i32, cap, &result);
        },
        audio.ma_format_f32 => {
            return Error.not_supported;
        },
        else => {
            return Error.unknown_format;
        },
    }
    return result;
}

fn decode_formatted_data(comptime T: type, alloc: std.mem.Allocator, len: usize, buffer: []const u8) ![]T {
    const tmp_buffer: []T = try alloc.alloc(T, len);
    var index: usize = 0;
    while (index < tmp_buffer.len) {
        tmp_buffer[index] = std.mem.readPackedInt(T, buffer[index..], 0, .little);
        index += @sizeOf(T);
    }
    return tmp_buffer;
}

fn cap_data_decode(alloc: std.mem.Allocator, cap: capture.CaptureData) !*audio.capture_data_t {
    var result: *audio.capture_data_t = audio.capture_data_create();
    result.sizeInFrames = @intCast(cap.sizeInFrames);
    result.format = @intCast(cap.format);
    result.channels = @intCast(cap.channels);
    result.buffer_len = cap.buffer.len / try get_format_size(result.format);
    switch (result.format) {
        audio.ma_format_u8 => {
            const tmp = try decode_formatted_data(u8, alloc, result.buffer_len, cap.buffer);
            result.buffer = @ptrCast(tmp.ptr);
        },
        audio.ma_format_s16 => {
            const tmp = try decode_formatted_data(i16, alloc, result.buffer_len, cap.buffer);
            result.buffer = @ptrCast(tmp.ptr);
        },
        audio.ma_format_s24 => {
            const tmp = try decode_formatted_data(i24, alloc, result.buffer_len, cap.buffer);
            result.buffer = @ptrCast(tmp.ptr);
        },
        audio.ma_format_s32 => {
            const tmp = try decode_formatted_data(i32, alloc, result.buffer_len, cap.buffer);
            result.buffer = @ptrCast(tmp.ptr);
        },
        audio.ma_format_f32 => {
            return Error.not_supported;
        },
        else => {
            return Error.unknown_format;
        },
    }
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
            continue;
        }
        if (cd_opt) |*cd| {
            defer audio.capture_data_destroy(@ptrCast(cd));
            if (cd.*.buffer) |_| {
                // TODO break up data into smaller packets
                // maybe, or maybe we should handle this in the audio lib
                var cap_data: capture.CaptureData = cap_data_encode(g_alloc.allocator(), cd.*) catch |err| {
                    std.debug.print("failed to encode capture_data: {any}\n", .{err});
                    continue;
                };
                defer cap_data.deinit();
                const marshal_data: []const u8 = cap_data.marshal() catch |err| {
                    std.debug.print("failed to marshal capture_data: {any}\n", .{err});
                    continue;
                };
                defer cap_data.alloc.free(marshal_data);
                if (chebi.message.Message.init_with_body(
                    g_alloc.allocator(),
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

fn create_capture() !void {
    const capture_opt = audio.capture_create(200);
    if (capture_opt == null) {
        return Error.audio_creation_failed;
    }
    if (capture_opt) |cap| {
        g_info.cap = cap;
    }
    _ = try std.Thread.spawn(.{
        .allocator = g_alloc.allocator(),
    }, handle_capture, .{&g_info});
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
    var conf = try config.config(g_alloc.allocator());
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
    var c = try client.Client.init(g_alloc.allocator(), addr);
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
            var data: capture.CaptureData = .init(g_alloc.allocator());
            defer data.deinit();
            try data.unmarshal(payload);
            var cd = try cap_data_decode(std.heap.c_allocator, data);
            const queue_result: audio.ma_result = audio.playback_queue(g_info.play, cd);
            if (queue_result != audio.MA_SUCCESS) {
                std.debug.print("playback_queue failed: code({})\n", .{queue_result});
            }
            audio.capture_data_destroy(@ptrCast(&cd));
        }
    }
}
