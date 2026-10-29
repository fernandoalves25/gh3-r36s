# Matches qbkey hashes (CRC32 of the lower-cased name, not inverted) against candidate field names.
#   python -I tools/qbkey_guess.py 783951d0 bd963c89 ...
import sys, zlib
def qb(s):
    s = s.lower().replace('/', chr(92))
    return (zlib.crc32(s.encode('latin1')) ^ 0xFFFFFFFF) & 0xFFFFFFFF
want = {int(a, 16) for a in sys.argv[1:]}
words = '''defaultunlocked default_unlocked unlock_count unlockcount num_songs encore boss name type count num_tiers numtiers first_tier next_tier prev_tier
venue level unlockedbyboss unlocked_by_boss unlocked_by_default completed complete tier1 tier2 tier3 tier4 tier5 tier6 tier7 tier8 tier_1 tier_2 tier_3 tier_4 tier_5
tier_6 tier_7 tier_8 unlockable locked is_boss isboss encores has_encore songlist songlist_type type_name tier_name tiername venue_name z_artdeco z_london z_newyork
z_ny z_dive z_backyard z_battle z_hell z_japan z_prison z_bighouse z_video z_shoot z_hotel z_europe z_general z_lou z_tom z_slash video_shoot z_artdeco_club
encore_count unlock_encore_count unlock unlocked start_unlocked startunlocked songs_unlocked unlock_all sp_unlock_count unlock_next_tier encore_songs_needed
defaultunlock default_unlock defaultunlocks unlock_default battle is_battle boss_battle bossbattle career_mode career singleplayer sp coop quickplay general bonus
download num_songs_to_unlock unlocknum num_unlock unlocked_count unlockedcount zone zonename zone_name level_name levelname next prev previous tier tiers num number
numsongs song_count songcount z_ny_club z_la z_london_club z_tokyo z_prison_yard z_hellfire z_dive_bar z_divebar z_backyard_bbq z_garage z_club z_arena z_stadium
z_festival z_desert z_rooftop singleplayer_01 singleplayer_02 singleplayer_03 singleplayer_04 singleplayer_05 singleplayer_06 singleplayer_07 singleplayer_08
singleplayer_end title songs unlockedbyencore unlocked_by_encore encore_unlocked unlock_encore completion_count complete_count songs_required required
num_required numrequired no_encore noencore no_boss noboss unlock_boss boss_unlock credits end bonus_songs general_songs download_songs career_songs
structure_name prefix suffix name_prefix next_tier_name prev_tier_name previous_tier next_setlist setlist setlist_name progression progress id index tier_index
tierindex tier_id video z_video_shoot z_shoot_video z_hotel_lobby z_artdeco_lobby z_prison_cell z_lou_hell z_battle_ground z_london_pub z_pub z_bar z_dive_club
z_backyard_party z_la_arena z_ny_arena z_japan_arena z_japan_club z_tokyo_club z_tokyo_arena z_hell_arena z_hell_club z_bighouse_prison z_bighouse_yard
unlock_tier unlocktier unlock_tiers tier_unlock tierunlock unlock_level unlock_zone zoneunlock z_general_club z_artdeco_arena z_european z_europe_arena
cover guitar_pro default_song defaultsong default_songs defaultsongs starting_songs startingsongs num_starting_songs initial initial_songs initial_unlocked
unlocked_initially num_unlocked_songs numunlockedsongs unlocked_songs unlockedsongs free free_songs freesongs open open_songs opened playable playable_songs
available available_songs always_unlocked alwaysunlocked unlocked_at_start unlockedatstart encore_song encoresong boss_song bosssong last_song lastsong
final_song finalsong career_name careername mode mode_name modename name_key namekey key menu_name menuname display_name displayname text label desc
description tier_text tiertext unlock_text unlocktext unlock_message unlockmessage'''.split()
for w in words:
    k = qb(w)
    if k in want: print('%08x = %s' % (k, w))
print('checked', len(words))
