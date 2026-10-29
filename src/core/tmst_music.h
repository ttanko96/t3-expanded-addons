/* TMST core: the game's "play music" call (0x8006AE14, a0 = the music id),
 * chained in front of the Expanded's own wrapper (its BGM SELECT), so that a
 * mod can see every piece the game starts and replace it. */
#ifndef TMST_MUSIC_H
#define TMST_MUSIC_H
#ifdef __cplusplus
extern "C" {
#endif

/* Called with the id the game asks for; returns the id to play instead,
 * -1 for silence (the call is skipped), or `id` itself to let it be. */
typedef int (*TmstMusicHook)(int id);

/* One hook (the Jukebox); a second registration replaces the first. */
void tmst_music_set_hook(TmstMusicHook hook);

#ifdef __cplusplus
}
#endif
#endif
