const std = @import("std");

pub const CaptureData = struct {
    alloc: std.mem.Allocator,
    sizeInFrames: u32,
    format: u8,
    channels: u8,
    buffer: []u8,

    pub fn init(alloc: std.mem.Allocator) CaptureData {
        const result: CaptureData = .{
            .alloc = alloc,
            .sizeInFrames = 0,
            .format = 0,
            .channels = 0,
            .buffer = &.{},
        };
        return result;
    }

    pub fn marshal_size(self: *const CaptureData) usize {
        return @sizeOf(u32) +
            @sizeOf(u8) + @sizeOf(u8) +
            @sizeOf(usize) + self.buffer.len;
    }

    pub fn marshal(self: *const CaptureData) ![]const u8 {
        const byteSize = self.marshal_size();
        const buffer: []u8 = try self.alloc.alloc(u8, byteSize);
        var offset: usize = 0;
        std.mem.writePackedInt(u32, buffer, offset, self.sizeInFrames, .little);
        offset += @sizeOf(u32);
        std.mem.writePackedInt(u8, buffer, offset, self.format, .little);
        offset += @sizeOf(u8);
        std.mem.writePackedInt(u8, buffer, offset, self.channels, .little);
        offset += @sizeOf(u8);
        std.mem.writePackedInt(usize, buffer, offset, self.buffer.len, .little);
        offset += @sizeOf(usize);
        @memcpy(buffer[offset..], self.buffer);
        return buffer;
    }

    pub fn unmarshal(self: *CaptureData, buffer: []const u8) !void {
        var offset: usize = 0;
        self.sizeInFrames = std.mem.readPackedInt(u32, buffer, offset, .little);
        offset += @sizeOf(u32);
        self.format = std.mem.readPackedInt(u8, buffer, offset, .little);
        offset += @sizeOf(u8);
        self.channels = std.mem.readPackedInt(u8, buffer, offset, .little);
        offset += @sizeOf(u8);
        const payload_len = std.mem.readPackedInt(usize, buffer, offset, .little);
        offset += @sizeOf(usize);
        self.buffer = try self.alloc.alloc(u8, payload_len);
        @memcpy(self.buffer, buffer[offset..]);
    }

    pub fn deinit(self: *CaptureData) void {
        self.alloc.free(self.buffer);
    }
};
