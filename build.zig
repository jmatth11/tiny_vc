const std = @import("std");

fn build_audio_lib(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) *std.Build.Module {
    const files: []const []const u8 = &.{
        "audio/src/audio.c",
        "audio/src/audio_types.c",
    };
    const flags: []const []const u8 = &.{
        "-Wall",
        "-std=c11",
    };
    const audio_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .pic = true,
        .link_libc = true,
    });
    audio_mod.addIncludePath(b.path("./audio/headers/"));
    audio_mod.addCSourceFiles(.{
        .files = files,
        .language = .c,
        .flags = flags,
    });
    audio_mod.linkSystemLibrary("m", .{.needed = true});
    audio_mod.linkSystemLibrary("pthread", .{.needed = true});
    audio_mod.linkSystemLibrary("atomic", .{.needed = true});
    return audio_mod;
}

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const mod = b.addModule("tiny_vc", .{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
    });

    const dep_opts = .{.target = target, .optimize = optimize,};
    const chebi = b.dependency("chebi", dep_opts).module("chebi");
    const clap = b.dependency("clap", dep_opts).module("clap");

    const exe = b.addExecutable(.{
        .name = "tiny_vc",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                // Here "tiny_vc" is the name you will use in your source code to
                // import this module (e.g. `@import("tiny_vc")`). The name is
                // repeated because you are allowed to rename your imports, which
                // can be extremely useful in case of collisions (which can happen
                // importing modules from different packages).
                .{ .name = "tiny_vc", .module = mod },
                .{ .name = "chebi", .module = chebi },
                .{ .name = "clap", .module = clap },
            },
        }),
    });
    // build and include audio lib
    const audio_mod = build_audio_lib(b, target, optimize);
    const audio_lib = b.addLibrary(.{
        .name = "audio",
        .root_module = audio_mod
    });
    exe.addIncludePath(b.path("./audio/headers/"));
    exe.linkLibrary(audio_lib);

    b.installArtifact(exe);

    const run_step = b.step("run", "Run the app");

    const run_cmd = b.addRunArtifact(exe);
    run_step.dependOn(&run_cmd.step);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const mod_tests = b.addTest(.{
        .root_module = mod,
    });

    const run_mod_tests = b.addRunArtifact(mod_tests);

    const exe_tests = b.addTest(.{
        .root_module = exe.root_module,
    });

    const run_exe_tests = b.addRunArtifact(exe_tests);

    const test_step = b.step("test", "Run tests");
    test_step.dependOn(&run_mod_tests.step);
    test_step.dependOn(&run_exe_tests.step);
}
