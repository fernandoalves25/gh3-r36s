/* English and Spanish for the play screens (lang.h): { Portuguese key as written in the code, English, Español }.
 * The English is GH3's own where the game has the words (qb.pak: "Amp Overload", "Powerup Steal", "Lefty Flip",
 * "Death Drain", "You Rock!!!"...); the Spanish is plain Spanish, to be polished by Spanish speakers. */
#include "../lang.h"
const lang_entry LANG_PLAY[] = {
    /* the HUD (render.c; PONTOS, SEM FALHAR and "%d seguidas" are lang_menus.c's rows) */
    { "Prepare-se!", "Get Ready!", "¡Prepárate!" },
    { "STAR POWER: %s", "STAR POWER: %s", "ENERGÍA ESTRELLA: %s" },
    { "%d NOTAS SEGUIDAS!", "%d NOTE STREAK!", "¡RACHA DE %d NOTAS!" },
    /* the battle's attacks (battle.c: what lands, what is won) */
    { "Amp Sobrecarregado", "Amp Overload", "Amplificador sobrecargado" },
    { "Dificuldade Maior", "Difficulty Up", "Más dificultad" },
    { "Notas Dobradas", "Double Notes", "Notas dobles" },
    { "Roubo de Poder", "Powerup Steal", "Robo de poder" },
    { "Inversão Canhota", "Lefty Flip", "Cambio a zurdo" },
    { "Inversão Destra", "Righty Flip", "Cambio a diestro" },
    { "Corda Arrebentada", "Broken String", "Cuerda rota" },
    { "Ataque de Alavanca", "Whammy", "Ataque de palanca" },
    { "Dreno Mortal", "Death Drain", "Drenaje mortal" },
    /* the battle's banners */
    { "Você é uma lenda do rock!", "You're a Legend of Rock!", "¡Eres una leyenda del rock!" },
    { "Agora acabe com ele!", "Now finish him off!", "¡Ahora acaba con él!" },
    { "Você arrasou!", "You Rock!", "¡Has arrasado!" },
    { "%s arrasou!", "%s Rocks!", "¡%s ha arrasado!" },
    /* the battle on screen (render_battle.c) */
    { "Alavanca!  %d", "Whammy!  %d", "¡Palanca!  %d" },
    { "Você", "You", "Tú" },
    { "ATACAR: %s", "ATTACK: %s", "ATACAR: %s" },
    { "Derrote %s!", "Defeat %s!", "¡Derrota a %s!" },
    { "BATALHA", "BATTLE", "BATALLA" },
    { "Lance o ataque do topo com o botão de star power (%s).", "Fire the top power-up with the star power button (%s).", "Lanza el ataque de arriba con el botón de energía estrella (%s)." },
    { "Faça %s falhar antes do fim da música!", "Make %s fail before the end of the song!", "¡Haz que %s falle antes del final de la canción!" },
    { "o chefe", "the boss", "el jefe" },
    { "Complete as notas de batalha para ganhar um ataque (até 3).", "Hit the battle gems to get a power-up (up to 3).", "Completa las notas de batalla para ganar un ataque (hasta 3)." },
    { "%s falhou: a batalha é sua!", "%s failed: the battle is yours!", "%s ha fallado: ¡la batalla es tuya!" },
    { "Toque o resto da música até o fim!", "Play the rest of the song to the end!", "¡Toca el resto de la canción hasta el final!" },
    { "O diabo foi derrotado!", "The Devil has been defeated!", "¡El diablo ha sido derrotado!" },
    { "Você não derrotou %s. Tente de novo!", "You didn't defeat %s. Try again!", "No has derrotado a %s. ¡Inténtalo de nuevo!" },
    { "Você venceu %s!", "You beat %s!", "¡Has vencido a %s!" },
    { "%s venceu a batalha", "%s won the battle", "%s ha ganado la batalla" },
    { "Ataques: %d seus, %d de %s • notas tocadas: %d de %d", "Attacks: %d yours, %d by %s • notes played: %d of %d", "Ataques: %d tuyos, %d de %s • notas tocadas: %d de %d" },
    { 0, 0, 0 }
};
