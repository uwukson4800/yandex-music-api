# ym_api

`ym_api.hpp` is a single-header C++ library for working with the Yandex Music API.

## Quick Start

```cpp
#define YM_IMPLEMENTATION
#include "ym_api.hpp"
```

Optional modules must be defined before the include:

```cpp
#define YM_IMPLEMENTATION
#define YM_WITH_PLAYER   // SDL2/SDL_mixer audio player
#define YM_WITH_YNISON   // real-time playback sync via WebSocket
#define YM_WITH_DISCORD  // Discord Rich Presence (Windows only)
#include "ym_api.hpp"
```

## Dependencies

- base : `libcurl`, `nlohmann/json`, `OpenSSL`
- `YM_WITH_PLAYER`: `SDL2`, `SDL2_mixer`
- `YM_WITH_YNISON`: `ixwebsocket`
- `YM_WITH_DISCORD`: Windows IPC only

## Authentication

```cpp
ym::client api("YOUR_TOKEN");

if (!api.is_authenticated()) {
    std::cerr << "Login failed\n";
    return 1;
}

std::cout << api.get_user_login() << "\n";
std::cout << api.get_user_display_name() << "\n";
std::cout << api.get_avatar_url() << "\n";
```

You can also login separately:

```cpp
ym::client api;
api.login("YOUR_TOKEN");
```

### Token Persistence

```cpp
api.save_token("session.dat");

ym::client api2;
std::string token = api2.load_token("session.dat");
if (!token.empty())
    api2.login(token);
```

## Search

```cpp
auto tracks = api.search_tracks("siouxxie", 0, 10);
for (const auto& t : tracks)
    std::cout << t.title << " - " << t.artist << "\n";

auto playlists = api.search_playlists("phonk", 0, 10);
for (const auto& pl : playlists)
    std::cout << pl.title << " by " << pl.owner << "\n";

auto artists = api.search_artists("The Weeknd");
for (const auto& a : artists)
    std::cout << a.name << "\n";
```

## Library

```cpp
// liked tracks (up to 100)
auto liked = api.liked_tracks();

// user playlists
auto playlists = api.user_playlists();

// tracks from a specific playlist
auto tracks = api.playlist_tracks("user_login", "playlist_kind");

// like / unlike
api.like_track("123456");
api.unlike_track("123456");
```

## Artists and Albums

```cpp
ym::artist_info_t info = api.get_artist("98765");
std::cout << info.info.name << "\n";
for (const auto& t : info.popular_tracks)
    std::cout << t.title << "\n";

auto tracks = api.artist_tracks("98765", 50);
auto album  = api.album_tracks("54321");
```

## Lyrics

```cpp
ym::lyrics_t lyrics = api.get_lyrics("123456");

if (lyrics.available) {
    if (lyrics.has_sync) {
        for (const auto& line : lyrics.lines)
            std::cout << line.time_sec << " -> " << line.text << "\n";
    } else {
        std::cout << lyrics.plain_text << "\n";
    }
}
```

## Wave / Radio

```cpp
// my wave
ym::wave_source_t src;
src.type = ym::wave_source_type_t::my_wave;

// wave based on a track
ym::wave_source_t src;
src.type = ym::wave_source_type_t::track;
src.id = "123456";

// wave based on an artist
ym::wave_source_t src;
src.type = ym::wave_source_type_t::artist;
src.id = "98765";

auto tracks = api.wave_tracks(src);
```

### Wave Settings

```cpp
ym::wave_settings_t settings;
settings.mood_energy = "active"; // fun, active, calm, sad, all
settings.diversity   = "discover"; // favorite, popular, discover, default
settings.language    = "any"; // russian, not-russian, any

api.wave_set_settings(src, settings);
```

### Feedback

Required by Yandex to keep wave recommendations working correctly.

```cpp
api.wave_start(src);

// when track finishes
api.wave_feedback(src, "trackFinished", api.wave_batch_id, "123456", 180.f);

// when track is skipped
api.wave_feedback(src, "skip", api.wave_batch_id, "123456", 30.f);
```

## Async Cover Loading

```cpp
ym::fetch_cover_async(track.cover(300), [](std::vector<uint8_t> data) {
    // data contains raw image bytes
});
```

Cover URLs contain a `%%` placeholder — use `cover(size)` to get a real URL:

```cpp
std::string url = track.cover(200);   // 200x200
std::string url = artist.cover(300);  // 300x300
```

## Player Module

Requires `YM_WITH_PLAYER`.

```cpp
ym::client api("YOUR_TOKEN");
ym::player player;

player.on_stream_url = [&](const std::string& id) {
    return api.stream_url(id, 320);
};

player.on_track_changed = [](const ym::track_t* t) {
    if (t) std::cout << "Now playing: " << t->title << " - " << t->artist << "\n";
};

player.on_state_changed = [](ym::player_state_t s) {
    if (s == ym::player_state_t::playing) std::cout << "Playing\n";
};

auto tracks = api.search_tracks("siouxxie", 0, 10);
player.set_queue(tracks, 0);
```

### Controls

```cpp
player.play();
player.pause();
player.resume();
player.stop();
player.next();
player.prev();           // restarts track if > 3s played, otherwise goes back
player.seek(0.5f);       // seek to 50%
player.seek_sec(90.f);   // seek to 1:30

player.set_volume(0.8f); // 0.0 - 1.0
player.set_repeat(ym::repeat_mode_t::all);  // none, one, all
player.set_shuffle(true);
```

### State

```cpp
player.get_state();      // stopped, playing, paused, loading
player.get_pos_sec();
player.get_dur_sec();
player.get_progress();   // 0.0 - 1.0
player.current_track();  // const track_t*
```

## Ynison Module

Real-time playback sync — mirrors what's playing across Yandex Music devices.

Requires `YM_WITH_YNISON`.

```cpp
ym::ynison sync("YOUR_TOKEN", "Yandex.Music Player"); // device name is optional, default: "YM-Lib"

sync.on_state_changed = [](const ym::ynison_state_t& s) {
    std::cout << "Track:    " << s.track_id << "\n";
    std::cout << "Progress: " << s.progress_ms << " ms\n";
    std::cout << "Paused:   " << s.paused << "\n";
};

sync.start();
// ...
sync.stop();
```

## Discord Rich Presence

Requires `YM_WITH_DISCORD` + `YM_WITH_PLAYER`. Windows only.

```cpp
ym::discord_rpc rpc("YOUR_DISCORD_APP_ID");
rpc.connect();

// call periodically (e.g. every second) to keep presence up to date
rpc.update(&player);
```

Or set presence manually:

```cpp
rpc.set_activity("Track Title", "by Artist", "cover_image_key", start_ms, end_ms);
rpc.clear_activity();
```

## Raw HTTP

For custom requests against the Yandex Music API:

```cpp
std::string resp = api.http_get("https://api.music.yandex.net/account/status");
std::string resp = api.http_post("https://api.music.yandex.net/some/endpoint", "param=value");
```

## Notes

- `login()` returns `false` on invalid token or network error.
- Most methods return an empty result on failure — no exceptions thrown.
- `wave_batch_id` is set automatically by `wave_tracks()` and should be passed to `wave_feedback()`.
- `liked_tracks()` fetches up to 100 tracks.

## Minimal Example

```cpp
#define YM_IMPLEMENTATION
#include "ym_api.hpp"
#include <iostream>

int main() {
    ym::client api("YOUR_TOKEN");

    auto tracks = api.search_tracks("siouxxie", 0, 5);
    for (const auto& t : tracks)
        std::cout << t.title << " - " << t.artist << "\n";
}
```

## Credits

- [Yandex](https://music.yandex.ru) — Yandex.Music service and API
- [MarshalX](https://github.com/MarshalX/yandex-music-api) — original Python library
- [uwukson4800](https://github.com/uwukson4800) — C++ port

## License

LGPL-3. You may use this library in proprietary applications without open-sourcing your code. Modifications to the library itself must be released under the same license.
