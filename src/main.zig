const std = @import("std");
const config = @import("config.zig");
const chebi = @import("chebi");

pub fn main() !void {
    const conf = try config.config(std.heap.smp_allocator);
}

