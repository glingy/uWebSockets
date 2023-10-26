const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const single_threaded = b.option(bool, "single_threaded", "Build single threaded") orelse false;

    const uSockets = b.dependency("uSockets", .{
        .target = target,
        .optimize = optimize,
        .single_threaded = single_threaded,
    });

    const zlib = b.dependency("zlib", .{
        .target = target,
        .optimize = optimize,
        .single_threaded = single_threaded,
    });

    const lib = b.addStaticLibrary(.{
        .name = "uWebSockets",
        .target = target,
        .optimize = optimize,
        .single_threaded = single_threaded,
        .root_source_file = .{ .path = "capi/libuwebsockets.cpp" },
    });

    if (single_threaded == true) {
        lib.defineCMacro("UWS_HAS_NO_THREADS", null);
        lib.defineCMacro("thread_local", ""); // Remove thread_local keywords
    }

    lib.addIncludePath(.{ .path = "capi" });
    lib.addIncludePath(.{ .path = "src" });

    lib.linkLibrary(zlib.artifact("z"));
    lib.linkLibrary(uSockets.artifact("uSockets"));
    lib.installLibraryHeaders(uSockets.artifact("uSockets"));

    lib.installHeader("capi/libuwebsockets.h", "libuwebsockets.h");

    b.installArtifact(lib);
}
