#pragma once

// A diagnostic (FrameLog = 1 in the ini; 0, the default, is off): where a frame's time goes. Once a second, from
// the tick, a line in the log:
// - the frames (the main thread's: sBioMain's frame, start to start), their average and the longest, and how many
//   took over 33 and over 100 ms;
// - the game's frame (sBioMain's own frame function, Present taken out when it runs inside it) and Present (the
//   device's: Direct3D - DXVK - taking the frame, and waiting when frames are ahead);
// - the game's draw thread (its EndScene): the scenes and the longest gap between two;
// - the mod's own time a frame: its tick, the loader's after-frame, its part of Present (the panel drawn there);
// - what the game made on the device that second (textures, buffers, shaders, vertex declarations), with the time
//   their calls took - under DXVK a new shader is a new Vulkan shader to compile.
// A frame over 100 ms gets a line of its own as it ends, its time split the same way (at most 10 a second).
// Nothing is hooked or timed while it is off, but the time counters' checks.
namespace re6cc::framelog {

bool on();               // FrameLog = 1
long long now_us();      // QueryPerformanceCounter, in microseconds

// The main thread's frame (dispatch.cpp): its start (which ends the frame before), the game's frame function's
// start and end (a Present inside it is taken out of the game's time), then the frame's own numbers - the mod's
// tick, the game's frame and the after-frame.
void begin(long long start);
void game_begin();
void game_end();
void end(long long tick_us, long long game_us, long long after_us);
// A Present (overlay_dx9.cpp): the mod's part before it, and the device's own.
void present(long long mod_us, long long present_us);
// The game's EndScene: its draw thread's (the thread of the first one; the panel's own scene is left out).
void end_scene(long long at);
// A device call that made something (any thread).
enum Kind { kTexture, kBuffer, kShader, kDeclaration, kKinds };
void made(Kind kind, long long us);
// The tick, every frame: once a second has passed since the last line, the line (`where`: the screen, the game).
void tick(long long now, const char* where);

}  // namespace re6cc::framelog
