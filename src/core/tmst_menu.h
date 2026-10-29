/* TMST core: rows of our own in Tekken 3's main menu, always at the end of
 * the list: after every row the game has and every row the Expanded adds
 * (its QUIT GAME), now or in a later release.
 *
 * The native menu reads a working list (count at +4, rows of 8 bytes from
 * +16: the label's address, then the mode byte and its flags). The Expanded
 * relocates that list and patches the menu's LUI/ORI (0x800DB5D0/D8) to its
 * copy; we read the list's address from those two instructions, so we never
 * need the Expanded's variables. The Expanded's menu code runs in its wrapper
 * of 0x80028CD4; ours is chained in front of it: our rows are taken out
 * before it runs (it always sees its own list) and put back at the end after
 * it, for the native menu that runs next. */
#ifndef TMST_MENU_H
#define TMST_MENU_H
#ifdef __cplusplus
extern "C" {
#endif

/* Called once when the row's choice is confirmed (the frame the game
 * dispatches its mode). */
typedef void (*TmstMenuChosen)(void);

/* A row labelled `label` (upper case, the menu's font) that dispatches the
 * mode of the first native row whose mode byte is `like_mode` (its look too):
 * Team Battle (2) for the Jukebox. Up to 2 rows. Returns the row's handle,
 * -1 when full. */
int tmst_menu_add_row(const char *label, unsigned like_mode, TmstMenuChosen chosen);
/* The menu comes back with this row highlighted (e.g. after its screen). */
void tmst_menu_select_next_time(int row);
/* Every VBlank, from the VBlank plugin of the mod that owns a row. */
void tmst_menu_vblank(void);

#ifdef __cplusplus
}
#endif
#endif
