/*
    ym_api.hpp - single-header C++ library

    usage:
        #define YM_IMPLEMENTATION
        #include "ym_api.hpp"

    optional modules ( define before include ):
        #define YM_WITH_PLAYER   - SDL2/SDL_mixer based audio player
        #define YM_WITH_YNISON   - real-time playback sync via WebSocket
        #define YM_WITH_DISCORD  - Discord Rich Presence via IPC ( Windows only )

    dependencies:
        libcurl, nlohmann/json, openssl
        YM_WITH_PLAYER  -> SDL2, SDL2_mixer
        YM_WITH_YNISON  -> ixwebsocket

    credits:
        Yandex      - Yandex.Music service and API
        MarshalX    - python library ( https://github.com/MarshalX/yandex-music-api )
        uwukson4800 - C++ port

    License: LGPL-3
*/

#pragma once
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <memory>
#include <optional>
#include <variant>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace ym {

    using json = nlohmann::json;

    // data types
    struct track_t {
        std::string id;
        std::string title;
        std::string version;
        std::string artist;
        std::string artist_id;
        std::string album;
        std::string album_id;
        std::string cover_url; // %% placeholder replaced with size
        int duration_ms = 0;
        bool is_liked = false;
        bool is_explicit = false;

        // cover_url with explicit size
        std::string cover( int size = 200 ) const;
    };

    struct playlist_t {
        std::string id;
        std::string kind;
        std::string title;
        std::string owner;
        std::string cover_url;
        int track_count = 0;

        std::string cover( int size = 300 ) const;
    };

    struct album_t {
        std::string id;
        std::string title;
        std::string cover_url;
        int year = 0;

        std::string cover( int size = 200 ) const;
    };

    struct artist_t {
        std::string id;
        std::string name;
        std::string cover_url;

        std::string cover( int size = 300 ) const;
    };

    struct artist_info_t {
        artist_t info;
        std::vector<track_t> popular_tracks;
        std::vector<album_t> albums;
    };

    struct lyrics_line_t {
        float time_sec = -1.f; // -1 = no timestamp
        std::string text;
    };

    struct lyrics_t {
        std::vector<lyrics_line_t> lines;
        std::string plain_text;
        bool has_sync = false; // true if lines have timestamps
        bool available = false;
    };

    struct wave_settings_t {
        std::string mood_energy = "all"; // fun, active, calm, sad, all
        std::string diversity = "favorite"; // favorite, popular, discover, default
        std::string language = "any"; // not-russian, russian, any
        // TODO: type | rotor, generative
    };

    enum class wave_source_type_t { my_wave, track, artist };

    struct wave_source_t {
        wave_source_type_t type = wave_source_type_t::my_wave;
        std::string id; // track id or artist id

        std::string station_id( ) const;
        std::string from_tag( ) const;
    };

    // error handling
    struct api_error_t : std::runtime_error {
        int http_code = 0;
        std::string raw_response;

        explicit api_error_t( const std::string& msg, int code = 0, std::string raw = "" )
            : std::runtime_error( msg ), http_code( code ), raw_response( std::move( raw ) ) {}
    };

    // client — main API entry point
    class client {
    public:
        explicit client( const std::string& token = "" );
        ~client( );

        // auth
        bool login( const std::string& token );
        void logout( );
        bool is_authenticated( ) const { return authenticated; }

        // token persistence
        bool save_token( const std::string& path = "session.dat" ) const;
        std::string load_token( const std::string& path = "session.dat" ) const;

        // account info
        const std::string& get_user_id( ) const { return user_id; }
        const std::string& get_user_login( ) const { return user_login; }
        const std::string& get_user_display_name( ) const { return user_display_name; }
        const std::string& get_avatar_url( ) const { return avatar_url; }
        std::string get_token( ) const { return token; }

        // search
        std::vector<track_t> search_tracks( const std::string& query, int page = 0, int page_size = 20 );
        std::vector<playlist_t> search_playlists( const std::string& query, int page = 0, int page_size = 20 );
        std::vector<artist_t> search_artists( const std::string& query, int page = 0, int page_size = 10 );

        // library
        std::vector<track_t> liked_tracks( );
        std::vector<playlist_t> user_playlists( );
        std::vector<track_t> playlist_tracks( const std::string& owner, const std::string& kind );

        // streaming
        std::string stream_url( const std::string& track_id, int preferred_bitrate = 192 );

        // likes
        bool like_track( const std::string& track_id );
        bool unlike_track( const std::string& track_id );

        // artist / album
        artist_info_t get_artist( const std::string& artist_id );
        std::vector<track_t> artist_tracks( const std::string& artist_id, int page_size = 100 );
        std::vector<track_t> album_tracks( const std::string& album_id );

        // lyrics
        lyrics_t get_lyrics( const std::string& track_id );

        // wave / radio
        std::vector<track_t> wave_tracks( const wave_source_t& src, const std::string& last_track_id = "" );
        bool wave_feedback( const wave_source_t& src, const std::string& type, const std::string& batch_id, const std::string& track_id = "", float played_sec = 0.f );
        bool wave_start( const wave_source_t& src );
        bool wave_set_settings( const wave_source_t& src, const wave_settings_t& s );

        std::string wave_batch_id;

        // raw HTTP helpers
        std::string http_get( const std::string& url );
        std::string http_post( const std::string& url, const std::string& body, bool json_content = false );

    private:
        std::string token;
        std::string user_id;
        std::string user_login;
        std::string user_display_name;
        std::string avatar_url;
        bool authenticated = false;

        static const std::string base_url;

        void parse_track( const json& j, track_t& t ) const;
        void fetch_avatar( );
        std::string cover_replace( const std::string& uri, const std::string& size ) const;
    };

    // async image loader
    using cover_callback_t = std::function<void( std::vector<uint8_t> data )>;
    void fetch_cover_async( const std::string& url, cover_callback_t on_done );

} // namespace ym

#ifdef YM_WITH_YNISON
#include <ixwebsocket/IXWebSocket.h>

namespace ym {

    struct ynison_state_t {
        std::string track_id;
        int current_index = -1;
        long long progress_ms = 0;
        long long duration_ms = 0;
        bool paused = true;
        float playback_speed = 1.f;
    };

    class ynison {
    public:
        ynison( const std::string& token, const std::string& device_name = "YM-Lib" );
        ~ynison( );

        void start( );
        void stop( );

        std::atomic<bool> running{ false };
        std::function<void( const ynison_state_t& )> on_state_changed;

    private:
        std::string token;
        std::string device_id;
        std::string device_name;
        std::string redirect_host;
        std::string redirect_ticket;
        std::string session_id;

        std::thread worker;

        bool get_redirect( );
        void connect_loop( );
        void handle_message( const std::string& raw );
        json build_initial_message( );

        static long long now_ms( );
        static std::string make_uuid( );
    };

} // namespace ym
#endif // YM_WITH_YNISON

#ifdef YM_WITH_PLAYER
#include <SDL2/SDL_mixer.h>

namespace ym {

    enum class repeat_mode_t { none, one, all };
    enum class player_state_t { stopped, playing, paused, loading };

    class player {
    public:
        player( );
        ~player( );

        void set_queue( const std::vector<track_t>& tracks, int start_index = 0 );
        void add_to_queue( const track_t& t );
        void clear_queue( );

        void play( );
        void pause( );
        void resume( );
        void stop( );
        void next( );
        void prev( );
        void seek( float percent );
        void seek_sec( float seconds );

        player_state_t get_state( ) const { return state.load( ); }
        float get_progress( ) const;
        float get_pos_sec( ) const;
        float get_dur_sec( ) const { return cached_dur.load( ); }
        int get_index( ) const { return cur_idx; }

        const track_t* current_track( ) const;
        const std::vector<track_t>& queue( ) const { return queue_list; }

        void set_volume( float v );
        float get_volume( ) const { return volume; }
        void set_repeat( repeat_mode_t m ) { repeat = m; }
        repeat_mode_t get_repeat( ) const { return repeat; }
        void set_shuffle( bool s );
        bool get_shuffle( ) const { return shuffle; }

        std::function<std::string( const std::string& track_id )> on_stream_url;
        std::function<void( const track_t* )> on_track_changed;
        std::function<void( player_state_t )> on_state_changed;

    private:
        void load_and_play( int index );
        int next_idx( ) const;
        int prev_idx( ) const;

        static void sdl_music_finished( );
        static player* s_instance;

        mutable std::mutex queue_mutex;
        std::vector<track_t> queue_list;
        std::vector<int> shuffle_order;
        int cur_idx = -1;

        mutable std::mutex audio_mutex;
        std::vector<char> audio_buf;
        Mix_Music* music = nullptr;

        std::atomic<player_state_t> state{ player_state_t::stopped };
        std::atomic<float> cached_dur{ 0.f };
        std::atomic<int> req_id{ 0 };
        std::atomic<bool> suppress_cb{ false };

        float volume = 0.8f;
        repeat_mode_t repeat = repeat_mode_t::none;
        bool shuffle = false;
    };

} // namespace ym
#endif // YM_WITH_PLAYER

#ifdef YM_WITH_DISCORD
#ifdef _WIN32

namespace ym {

    class discord_rpc {
    public:
        explicit discord_rpc( const std::string& app_id );
        ~discord_rpc( );

        void connect( );
        void disconnect( );
        bool is_connected( ) const { return connected; }

#ifdef YM_WITH_PLAYER
        void update( player* p );
#endif
        void set_activity( const std::string& details, const std::string& state,
            const std::string& large_image = "",
            int64_t start_ms = 0, int64_t end_ms = 0 );
        void clear_activity( );

    private:
        std::string app_id;
        void* pipe = nullptr;
        bool connected = false;
        int nonce = 0;

        bool ipc_connect( );
        void ipc_disconnect( );
        bool ipc_write( const std::string& payload, int opcode = 1 );
        std::string ipc_read( );

        std::string escape_json( const std::string& s );
        std::string build_handshake( );

#ifdef YM_WITH_PLAYER
        std::string last_track_id;
        bool last_paused = false;
        int64_t last_update = 0;
#endif
    };

} // namespace ym
#endif // _WIN32
#endif // YM_WITH_DISCORD

// implementation (compiled only when YM_IMPLEMENTATION is defined)
#ifdef YM_IMPLEMENTATION

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <random>
#include <ctime>
#include <climits>
#include <iostream>

namespace ym {

    // helpers
    static size_t curl_write_str( void* ptr, size_t size, size_t nmemb, std::string* out ) {
        out->append( static_cast< char* >( ptr ), size * nmemb );
        return size * nmemb;
    }

    static size_t curl_write_vec( void* ptr, size_t size, size_t nmemb, std::vector<uint8_t>* out ) {
        auto* p = static_cast< uint8_t* >( ptr );
        out->insert( out->end( ), p, p + size * nmemb );
        return size * nmemb;
    }

    static std::string md5_hex( const std::string& s ) {
        unsigned char hash[ EVP_MAX_MD_SIZE ];
        unsigned int len = 0;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new( );
        if ( EVP_DigestInit_ex( ctx, EVP_md5( ), nullptr ) ) {
            EVP_DigestUpdate( ctx, s.data( ), s.size( ) );
            EVP_DigestFinal_ex( ctx, hash, &len );
        }
        EVP_MD_CTX_free( ctx );

        std::ostringstream ss;
        for ( unsigned i = 0; i < len; i++ )
            ss << std::hex << std::setw( 2 ) << std::setfill( '0' ) << ( int )hash[ i ];
        return ss.str( );
    }

    static std::string hmac_sha256_b64( const std::string& key, const std::string& data ) {
        unsigned char hash[ EVP_MAX_MD_SIZE ];
        unsigned int len = 0;
        HMAC( EVP_sha256( ),
            key.data( ), ( int )key.size( ),
            ( const unsigned char* )data.data( ), ( int )data.size( ),
            hash, &len );

        static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve( ( ( len + 2 ) / 3 ) * 4 );
        for ( unsigned i = 0; i < len; i += 3 ) {
            unsigned b = ( hash[ i ] << 16 )
                | ( i + 1 < len ? hash[ i + 1 ] << 8 : 0 )
                | ( i + 2 < len ? hash[ i + 2 ] : 0 );
            out += chars[ ( b >> 18 ) & 0x3f ];
            out += chars[ ( b >> 12 ) & 0x3f ];
            out += ( i + 1 < len ) ? chars[ ( b >> 6 ) & 0x3f ] : '=';
            out += ( i + 2 < len ) ? chars[ b & 0x3f ] : '=';
        }
        return out;
    }

    static std::string replace_placeholder( const std::string& uri, const std::string& size ) {
        std::string out = uri;
        auto pos = out.find( "%%" );
        if ( pos != std::string::npos )
            out.replace( pos, 2, size );
        return out;
    }

    static std::string normalize_cover( const std::string& raw ) {
        if ( raw.empty( ) )
            return {};
        std::string uri = raw;
        if ( uri.rfind( "//", 0 ) == 0 )
            uri = "https:" + uri;
        else if ( uri.find( "http" ) != 0 )
            uri = "https://" + uri;
        return uri;
    }

    static std::string json_id_str( const json& j ) {
        if ( j.is_number( ) )
            return std::to_string( j.get<long long>( ) );
        if ( j.is_string( ) )
            return j.get<std::string>( );
        return {};
    }

    // data type helpers
    std::string track_t::cover( int size ) const {
        return replace_placeholder( normalize_cover( cover_url ), std::to_string( size ) + "x" + std::to_string( size ) );
    }

    std::string playlist_t::cover( int size ) const {
        return replace_placeholder( normalize_cover( cover_url ), std::to_string( size ) + "x" + std::to_string( size ) );
    }

    std::string album_t::cover( int size ) const {
        return replace_placeholder( normalize_cover( cover_url ), std::to_string( size ) + "x" + std::to_string( size ) );
    }

    std::string artist_t::cover( int size ) const {
        return replace_placeholder( normalize_cover( cover_url ), std::to_string( size ) + "x" + std::to_string( size ) );
    }

    std::string wave_source_t::station_id( ) const {
        switch ( type ) {
        case wave_source_type_t::track:  return "track:" + id;
        case wave_source_type_t::artist: return "artist:" + id;
        default:                         return "user:onyourwave";
        }
    }

    std::string wave_source_t::from_tag( ) const {
        switch ( type ) {
        case wave_source_type_t::track:  return "track-" + id;
        case wave_source_type_t::artist: return "artist-" + id;
        default:                         return "desktop-win-home-user:onyourwave";
        }
    }

    // client
    const std::string client::base_url = "https://api.music.yandex.net";

    client::client( const std::string& token ) {
        if ( !token.empty( ) )
            login( token );
    }

    client::~client( ) = default;

    bool client::login( const std::string& token ) {
        this->token = token;
        auto resp = json::parse( http_get( base_url + "/account/status" ) );
        if ( !resp.contains( "result" ) )
            return false;

        auto& acc = resp[ "result" ][ "account" ];
        user_id = json_id_str( acc[ "uid" ] );
        user_login = acc[ "login" ].get<std::string>( );

        user_display_name = user_login;
        if ( acc.contains( "displayName" ) && !acc[ "displayName" ].is_null( ) )
            user_display_name = acc[ "displayName" ].get<std::string>( );
        else if ( acc.contains( "fullName" ) && !acc[ "fullName" ].is_null( ) )
            user_display_name = acc[ "fullName" ].get<std::string>( );

        if ( acc.contains( "avatarUri" ) && !acc[ "avatarUri" ].is_null( ) ) {
            avatar_url = "https://" + acc[ "avatarUri" ].get<std::string>( );
            auto pos = avatar_url.find( "%%" );
            if ( pos != std::string::npos )
                avatar_url.replace( pos, 2, "islands-200" );
        }
        else {
            fetch_avatar( );
        }

        authenticated = true;
        return true;
    }

    void client::logout( ) {
        token.clear( );
        user_id.clear( );
        user_login.clear( );
        user_display_name.clear( );
        avatar_url.clear( );
        wave_batch_id.clear( );
        authenticated = false;
    }

    bool client::save_token( const std::string& path ) const {
        std::ofstream f( path );
        if ( !f )
            return false;
        f << token;
        return true;
    }

    std::string client::load_token( const std::string& path ) const {
        std::ifstream f( path );
        if ( !f )
            return {};
        std::string tok;
        f >> tok;
        return tok;
    }

    void client::fetch_avatar( ) {
        CURL* c = curl_easy_init( );
        if ( !c )
            return;

        std::string resp;
        std::string auth = "Authorization: OAuth " + token;
        curl_slist* hdrs = curl_slist_append( nullptr, auth.c_str( ) );

        curl_easy_setopt( c, CURLOPT_URL, "https://login.yandex.ru/info?format=json" );
        curl_easy_setopt( c, CURLOPT_HTTPHEADER, hdrs );
        curl_easy_setopt( c, CURLOPT_FOLLOWLOCATION, 1L );
        curl_easy_setopt( c, CURLOPT_SSL_VERIFYPEER, 0L );
        curl_easy_setopt( c, CURLOPT_TIMEOUT, 10L );
        curl_easy_setopt( c, CURLOPT_WRITEFUNCTION, curl_write_str );
        curl_easy_setopt( c, CURLOPT_WRITEDATA, &resp );
        curl_easy_perform( c );
        curl_slist_free_all( hdrs );
        curl_easy_cleanup( c );

        if ( resp.empty( ) )
            return;
        auto j = json::parse( resp );
        bool has_avatar = !j.value( "is_avatar_empty", true );
        if ( has_avatar && j.contains( "default_avatar_id" ) )
            avatar_url = "https://avatars.yandex.net/get-yapic/" + j[ "default_avatar_id" ].get<std::string>( ) + "/islands-200";
    }

    void client::parse_track( const json& j, track_t& t ) const {
        t.id = json_id_str( j[ "id" ] );
        t.title = j[ "title" ].get<std::string>( );
        t.version = j.value( "version", "" );
        t.is_explicit = j.contains( "explicit" ) && j[ "explicit" ].is_boolean( ) && j[ "explicit" ].get<bool>( );

        if ( j.contains( "artists" ) && !j[ "artists" ].empty( ) ) {
            t.artist = j[ "artists" ][ 0 ][ "name" ].get<std::string>( );
            t.artist_id = json_id_str( j[ "artists" ][ 0 ][ "id" ] );
        }

        if ( j.contains( "albums" ) && !j[ "albums" ].empty( ) ) {
            t.album = j[ "albums" ][ 0 ].value( "title", "" );
            t.album_id = json_id_str( j[ "albums" ][ 0 ][ "id" ] );
        }

        if ( j.contains( "durationMs" ) )
            t.duration_ms = j[ "durationMs" ].get<int>( );

        if ( j.contains( "coverUri" ) && !j[ "coverUri" ].is_null( ) )
            t.cover_url = normalize_cover( j[ "coverUri" ].get<std::string>( ) );
    }

    std::string client::cover_replace( const std::string& uri, const std::string& size ) const {
        return replace_placeholder( uri, size );
    }

    // HTTP helpers
    std::string client::http_get( const std::string& url ) {
        CURL* c = curl_easy_init( );
        std::string resp;
        if ( !c )
            return resp;

        curl_slist* hdrs = nullptr;
        hdrs = curl_slist_append( hdrs, ( "Authorization: OAuth " + token ).c_str( ) );
        hdrs = curl_slist_append( hdrs, "X-Yandex-Music-Client: YandexMusicAndroid/23020251" );
        hdrs = curl_slist_append( hdrs, "Accept: application/json" );

        curl_easy_setopt( c, CURLOPT_URL, url.c_str( ) );
        curl_easy_setopt( c, CURLOPT_HTTPHEADER, hdrs );
        curl_easy_setopt( c, CURLOPT_WRITEFUNCTION, curl_write_str );
        curl_easy_setopt( c, CURLOPT_WRITEDATA, &resp );
        curl_easy_setopt( c, CURLOPT_SSL_VERIFYPEER, 1L );
        curl_easy_setopt( c, CURLOPT_TIMEOUT, 15L );
        curl_easy_perform( c );
        curl_slist_free_all( hdrs );
        curl_easy_cleanup( c );
        return resp;
    }

    std::string client::http_post( const std::string& url, const std::string& body, bool json_ct ) {
        CURL* c = curl_easy_init( );
        std::string resp;
        if ( !c )
            return resp;

        curl_slist* hdrs = nullptr;
        hdrs = curl_slist_append( hdrs, ( "Authorization: OAuth " + token ).c_str( ) );
        hdrs = curl_slist_append( hdrs, json_ct ? "Content-Type: application/json" : "Content-Type: application/x-www-form-urlencoded" );
        hdrs = curl_slist_append( hdrs, "X-Yandex-Music-Client: YandexMusicAndroid/23020251" );

        curl_easy_setopt( c, CURLOPT_URL, url.c_str( ) );
        curl_easy_setopt( c, CURLOPT_HTTPHEADER, hdrs );
        curl_easy_setopt( c, CURLOPT_POSTFIELDS, body.c_str( ) );
        curl_easy_setopt( c, CURLOPT_WRITEFUNCTION, curl_write_str );
        curl_easy_setopt( c, CURLOPT_WRITEDATA, &resp );
        curl_easy_setopt( c, CURLOPT_TIMEOUT, 15L );
        curl_easy_perform( c );
        curl_slist_free_all( hdrs );
        curl_easy_cleanup( c );
        return resp;
    }

    // search
    std::vector<track_t> client::search_tracks( const std::string& query, int page, int page_size ) {
        std::vector<track_t> result;
        char* esc = curl_easy_escape( nullptr, query.c_str( ), ( int )query.size( ) );
        std::string url = base_url + "/search?type=track&text=" + esc + "&page=" + std::to_string( page ) + "&pageSize=" + std::to_string( page_size );
        curl_free( esc );

        auto j = json::parse( http_get( url ) );
        for ( auto& jt : j[ "result" ][ "tracks" ][ "results" ] ) {
            track_t t;
            parse_track( jt, t );
            result.push_back( std::move( t ) );
        }
        return result;
    }

    std::vector<playlist_t> client::search_playlists( const std::string& query, int page, int page_size ) {
        std::vector<playlist_t> result;
        char* esc = curl_easy_escape( nullptr, query.c_str( ), ( int )query.size( ) );
        std::string url = base_url + "/search?type=playlist&text=" + esc + "&page=" + std::to_string( page ) + "&pageSize=" + std::to_string( page_size );
        curl_free( esc );

        auto j = json::parse( http_get( url ) );
        for ( auto& jp : j[ "result" ][ "playlists" ][ "results" ] ) {
            playlist_t pl;
            pl.id = json_id_str( jp[ "kind" ] );
            pl.kind = pl.id;
            pl.title = jp[ "title" ].get<std::string>( );
            pl.owner = jp[ "owner" ][ "login" ].get<std::string>( );
            if ( jp.contains( "cover" ) && jp[ "cover" ].contains( "uri" ) )
                pl.cover_url = normalize_cover( jp[ "cover" ][ "uri" ].get<std::string>( ) );
            result.push_back( std::move( pl ) );
        }
        return result;
    }

    std::vector<artist_t> client::search_artists( const std::string& query, int page, int page_size ) {
        std::vector<artist_t> result;
        char* esc = curl_easy_escape( nullptr, query.c_str( ), ( int )query.size( ) );
        std::string url = base_url + "/search?type=artist&text=" + esc + "&page=" + std::to_string( page ) + "&pageSize=" + std::to_string( page_size );
        curl_free( esc );

        auto j = json::parse( http_get( url ) );
        for ( auto& ja : j[ "result" ][ "artists" ][ "results" ] ) {
            artist_t a;
            a.id = json_id_str( ja[ "id" ] );
            a.name = ja[ "name" ].get<std::string>( );
            if ( ja.contains( "cover" ) && !ja[ "cover" ].is_null( ) && ja[ "cover" ].contains( "uri" ) )
                a.cover_url = normalize_cover( ja[ "cover" ][ "uri" ].get<std::string>( ) );
            result.push_back( std::move( a ) );
        }
        return result;
    }

    // library
    std::vector<track_t> client::liked_tracks( ) {
        std::vector<track_t> result;
        if ( user_id.empty( ) )
            return result;

        auto j = json::parse( http_get( base_url + "/users/" + user_id + "/likes/tracks" ) );
        if ( !j.contains( "result" ) )
            return result;

        auto& lib = j[ "result" ][ "library" ][ "tracks" ];
        std::string ids;
        for ( int i = 0; i < ( int )lib.size( ) && i < 100; i++ ) {
            if ( !ids.empty( ) )
                ids += ",";
            ids += json_id_str( lib[ i ][ "id" ] );
        }
        if ( ids.empty( ) )
            return result;

        auto jd = json::parse( http_post( base_url + "/tracks", "track-ids=" + ids ) );
        for ( auto& jt : jd[ "result" ] ) {
            track_t t;
            parse_track( jt, t );
            t.is_liked = true;
            result.push_back( std::move( t ) );
        }
        return result;
    }

    std::vector<playlist_t> client::user_playlists( ) {
        std::vector<playlist_t> result;
        auto j = json::parse( http_get( base_url + "/users/" + user_id + "/playlists/list" ) );
        for ( auto& jp : j[ "result" ] ) {
            playlist_t pl;
            pl.id = std::to_string( jp[ "kind" ].get<long long>( ) );
            pl.kind = pl.id;
            pl.title = jp[ "title" ].get<std::string>( );
            pl.owner = user_login;
            if ( jp.contains( "cover" ) && jp[ "cover" ].contains( "uri" ) )
                pl.cover_url = normalize_cover( jp[ "cover" ][ "uri" ].get<std::string>( ) );
            result.push_back( std::move( pl ) );
        }
        return result;
    }

    std::vector<track_t> client::playlist_tracks( const std::string& owner, const std::string& kind ) {
        std::vector<track_t> result;
        auto j = json::parse( http_get( base_url + "/users/" + owner + "/playlists/" + kind ) );
        if ( !j.contains( "result" ) || !j[ "result" ].contains( "tracks" ) )
            return result;

        for ( auto& item : j[ "result" ][ "tracks" ] ) {
            auto& jt = item.contains( "track" ) ? item[ "track" ] : item;
            if ( jt.is_null( ) )
                continue;
            track_t t;
            parse_track( jt, t );
            result.push_back( std::move( t ) );
        }
        return result;
    }

    // streaming
    static const std::string track_sign_secret = "XGRlBW9FXlekgbPrRHuSiA";

    std::string client::stream_url( const std::string& track_id, int preferred_bitrate ) {
        auto j = json::parse( http_get( base_url + "/tracks/" + track_id + "/download-info" ) );

        std::string best_url;
        int best_br = -1;
        int best_above = INT_MAX;
        std::string above_url;

        for ( auto& e : j[ "result" ] ) {
            if ( e[ "codec" ] != "mp3" || e[ "preview" ].get<bool>( ) )
                continue;
            int br = e[ "bitrateInKbps" ].get<int>( );
            std::string eu = e[ "downloadInfoUrl" ].get<std::string>( );

            if ( br == preferred_bitrate ) {
                best_url = eu;
                break;
            }
            if ( br < preferred_bitrate && br > best_br ) {
                best_br = br;
                best_url = eu;
            }
            else if ( br > preferred_bitrate && br < best_above ) {
                best_above = br;
                above_url = eu;
            }
        }

        if ( best_url.empty( ) )
            best_url = above_url;
        if ( best_url.empty( ) )
            return {};

        std::string xml = http_get( best_url );
        auto tag = [ & ]( const std::string& name ) {
            size_t s = xml.find( "<" + name + ">" ) + name.size( ) + 2;
            size_t e = xml.find( "</" + name + ">" );
            return xml.substr( s, e - s );
            };

        std::string path = tag( "path" );
        std::string sign = md5_hex( track_sign_secret + path.substr( 1 ) + tag( "s" ) );
        return "https://" + tag( "host" ) + "/get-mp3/" + sign + "/" + tag( "ts" ) + path;
    }

    // likes
    bool client::like_track( const std::string& track_id ) {
        auto j = json::parse( http_post( base_url + "/users/" + user_id + "/likes/tracks/add-multiple", "track-ids=" + track_id ) );
        return j.contains( "result" );
    }

    bool client::unlike_track( const std::string& track_id ) {
        auto j = json::parse( http_post( base_url + "/users/" + user_id + "/likes/tracks/remove", "track-ids=" + track_id ) );
        return j.contains( "result" );
    }

    // artist / album
    artist_info_t client::get_artist( const std::string& artist_id ) {
        artist_info_t result;
        result.info.id = artist_id;
        auto j = json::parse( http_get( base_url + "/artists/" + artist_id + "/brief-info" ) );
        if ( !j.contains( "result" ) )
            return result;
        auto& res = j[ "result" ];

        if ( res.contains( "artist" ) ) {
            result.info.name = res[ "artist" ][ "name" ].get<std::string>( );
            if ( res[ "artist" ].contains( "cover" ) && !res[ "artist" ][ "cover" ][ "uri" ].is_null( ) )
                result.info.cover_url = normalize_cover( res[ "artist" ][ "cover" ][ "uri" ].get<std::string>( ) );
        }

        if ( res.contains( "popularTracks" ) ) {
            for ( auto& jt : res[ "popularTracks" ] ) {
                track_t t;
                parse_track( jt, t );
                result.popular_tracks.push_back( std::move( t ) );
            }
        }

        if ( res.contains( "albums" ) ) {
            for ( auto& ja : res[ "albums" ] ) {
                album_t a;
                a.id = json_id_str( ja[ "id" ] );
                a.title = ja[ "title" ].get<std::string>( );
                if ( ja.contains( "year" ) )
                    a.year = ja[ "year" ].get<int>( );
                if ( ja.contains( "coverUri" ) && !ja[ "coverUri" ].is_null( ) )
                    a.cover_url = normalize_cover( ja[ "coverUri" ].get<std::string>( ) );
                result.albums.push_back( std::move( a ) );
            }
        }
        return result;
    }

    std::vector<track_t> client::artist_tracks( const std::string& artist_id, int page_size ) {
        std::vector<track_t> result;
        auto j = json::parse( http_get( base_url + "/artists/" + artist_id + "/tracks?page=0&pageSize=" + std::to_string( page_size ) ) );
        if ( j.contains( "result" ) && j[ "result" ].contains( "tracks" ) ) {
            for ( auto& jt : j[ "result" ][ "tracks" ] ) {
                track_t t;
                parse_track( jt, t );
                result.push_back( std::move( t ) );
            }
        }
        return result;
    }

    std::vector<track_t> client::album_tracks( const std::string& album_id ) {
        std::vector<track_t> result;
        auto j = json::parse( http_get( base_url + "/albums/" + album_id + "/with-tracks" ) );
        if ( j.contains( "result" ) && j[ "result" ].contains( "volumes" ) ) {
            for ( auto& vol : j[ "result" ][ "volumes" ] ) {
                for ( auto& jt : vol ) {
                    track_t t;
                    parse_track( jt, t );
                    result.push_back( std::move( t ) );
                }
            }
        }
        return result;
    }

    // lyrics
    static std::string lyrics_sign_secret = "p93jhgh689SBReK6ghtw62";

    lyrics_t client::get_lyrics( const std::string& track_id ) {
        lyrics_t result;
        auto j = json::parse( http_get( base_url + "/tracks/" + track_id + "/supplement" ) );
        if ( j.contains( "result" ) && j[ "result" ].contains( "lyrics" ) && !j[ "result" ][ "lyrics" ].is_null( ) ) {
            auto& lyr = j[ "result" ][ "lyrics" ];
            for ( auto& key : { "fullLyrics", "text" } ) {
                if ( lyr.contains( key ) && lyr[ key ].is_string( ) ) {
                    result.plain_text = lyr[ key ].get<std::string>( );
                    if ( !result.plain_text.empty( ) ) {
                        result.available = true;
                        return result;
                    }
                }
            }
        }

        std::string num = track_id.substr( 0, track_id.find( ':' ) );
        std::time_t ts = std::time( nullptr );
        std::string sign_src = num + std::to_string( ts );
        std::string sign = hmac_sha256_b64( lyrics_sign_secret, sign_src );

        char* sign_esc = curl_easy_escape( nullptr, sign.c_str( ), ( int )sign.size( ) );
        std::string url = base_url + "/tracks/" + track_id + "/lyrics?format=LRC&timeStamp=" + std::to_string( ts ) + "&sign=" + ( sign_esc ? sign_esc : sign );
        if ( sign_esc )
            curl_free( sign_esc );

        auto jl = json::parse( http_get( url ) );
        if ( !jl.contains( "result" ) )
            return result;
        auto& lr = jl[ "result" ];

        if ( lr.contains( "downloadUrl" ) ) {
            std::string dl = lr[ "downloadUrl" ].get<std::string>( );
            CURL* c = curl_easy_init( );
            std::string raw;
            if ( c ) {
                curl_easy_setopt( c, CURLOPT_URL, dl.c_str( ) );
                curl_easy_setopt( c, CURLOPT_FOLLOWLOCATION, 1L );
                curl_easy_setopt( c, CURLOPT_PATH_AS_IS, 1L );
                curl_easy_setopt( c, CURLOPT_SSL_VERIFYPEER, 0L );
                curl_easy_setopt( c, CURLOPT_TIMEOUT, 10L );
                curl_easy_setopt( c, CURLOPT_WRITEFUNCTION, curl_write_str );
                curl_easy_setopt( c, CURLOPT_WRITEDATA, &raw );
                curl_easy_perform( c );
                curl_easy_cleanup( c );
            }

            if ( !raw.empty( ) ) {
                std::istringstream iss( raw );
                std::string line;
                while ( std::getline( iss, line ) ) {
                    if ( !line.empty( ) && line.back( ) == '\r' )
                        line.pop_back( );
                    lyrics_line_t ll;
                    if ( line.size( ) > 6 && line[ 0 ] == '[' ) {
                        size_t close = line.find( ']' );
                        if ( close != std::string::npos ) {
                            std::string tstr = line.substr( 1, close - 1 );
                            int mm = 0;
                            float ss = 0.f;
                            if ( sscanf( tstr.c_str( ), "%d:%f", &mm, &ss ) == 2 )
                                ll.time_sec = mm * 60.f + ss;
                            std::string txt = line.substr( close + 1 );
                            auto s = txt.find_first_not_of( " \t" );
                            ll.text = ( s != std::string::npos ) ? txt.substr( s ) : "";
                        }
                    }
                    else {
                        auto s = line.find_first_not_of( " \t" );
                        ll.text = ( s != std::string::npos ) ? line.substr( s ) : "";
                    }
                    result.lines.push_back( ll );
                    result.plain_text += ll.text + "\n";
                }
                result.has_sync = true;
                result.available = !result.lines.empty( );
                return result;
            }
        }

        if ( lr.contains( "fullLyrics" ) ) {
            result.plain_text = lr[ "fullLyrics" ].get<std::string>( );
            result.available = !result.plain_text.empty( );
        }
        return result;
    }

    // wave / radio
    std::vector<track_t> client::wave_tracks( const wave_source_t& src, const std::string& last_track_id ) {
        std::vector<track_t> result;
        std::string url = base_url + "/rotor/station/" + src.station_id( ) + "/tracks?settings2=true";
        if ( !last_track_id.empty( ) )
            url += "&queue=" + last_track_id;

        auto j = json::parse( http_get( url ) );
        if ( !j.contains( "result" ) )
            return result;
        auto& res = j[ "result" ];

        if ( res.contains( "batchId" ) )
            wave_batch_id = res[ "batchId" ].get<std::string>( );

        for ( auto& item : res[ "sequence" ] ) {
            if ( !item.contains( "track" ) )
                continue;
            track_t t;
            parse_track( item[ "track" ], t );
            result.push_back( std::move( t ) );
        }
        return result;
    }

    bool client::wave_feedback( const wave_source_t& src, const std::string& type, const std::string& batch_id, const std::string& track_id, float played_sec ) {
        json body;
        body[ "type" ] = type;
        body[ "timestamp" ] = ( double )std::time( nullptr );
        body[ "from" ] = src.from_tag( );
        if ( !batch_id.empty( ) )
            body[ "batchId" ] = batch_id;
        if ( !track_id.empty( ) )
            body[ "trackId" ] = std::stoll( track_id );
        if ( type == "trackFinished" || type == "skip" )
            body[ "totalPlayedSeconds" ] = played_sec;

        std::string url = base_url + "/rotor/station/" + src.station_id( ) + "/feedback";
        if ( !batch_id.empty( ) )
            url += "?batch-id=" + batch_id;

        auto j = json::parse( http_post( url, body.dump( ), true ) );
        return j.contains( "result" );
    }

    bool client::wave_start( const wave_source_t& src ) {
        return wave_feedback( src, "radioStarted", wave_batch_id );
    }

    bool client::wave_set_settings( const wave_source_t& src, const wave_settings_t& s ) {
        json body;
        body[ "moodEnergy" ] = s.mood_energy;
        body[ "diversity" ] = s.diversity;
        body[ "language" ] = s.language;

        ( void )src;
        auto j = json::parse( http_post( base_url + "/rotor/station/user:onyourwave/settings2", body.dump( ), true ) );
        return j.contains( "result" );
    }

    // async cover fetch
    void fetch_cover_async( const std::string& url, cover_callback_t on_done ) {
        if ( url.empty( ) || !on_done )
            return;
        std::thread( [ url, on_done ]( ) {
            CURL* c = curl_easy_init( );
            if ( !c )
                return;
            std::vector<uint8_t> buf;
            curl_easy_setopt( c, CURLOPT_URL, url.c_str( ) );
            curl_easy_setopt( c, CURLOPT_FOLLOWLOCATION, 1L );
            curl_easy_setopt( c, CURLOPT_TIMEOUT, 10L );
            curl_easy_setopt( c, CURLOPT_WRITEFUNCTION, curl_write_vec );
            curl_easy_setopt( c, CURLOPT_WRITEDATA, &buf );
            if ( curl_easy_perform( c ) == CURLE_OK && !buf.empty( ) )
                on_done( std::move( buf ) );
            curl_easy_cleanup( c );
            } ).detach( );
    }

#ifdef YM_WITH_YNISON
    static std::string ynison_make_uuid( ) {
        static std::random_device rd;
        static std::mt19937 gen( rd( ) );
        static std::uniform_int_distribution<> dis( 0, 15 );
        const char* hex = "0123456789abcdef";
        std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
        for ( auto& ch : uuid ) {
            if ( ch == 'x' )
                ch = hex[ dis( gen ) ];
            else if ( ch == 'y' )
                ch = hex[ ( dis( gen ) & 0x3 ) | 0x8 ];
        }
        return uuid;
    }

    long long ynison::now_ms( ) {
        return std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::system_clock::now( ).time_since_epoch( ) ).count( );
    }

    std::string ynison::make_uuid( ) {
        return ynison_make_uuid( );
    }

    ynison::ynison( const std::string& token, const std::string& device_name ) : token( token ), device_name( device_name ) {
        device_id = ynison_make_uuid( );
    }

    ynison::~ynison( ) {
        stop( );
    }

    void ynison::start( ) {
        running = true;
        worker = std::thread( [ this ]( ) { connect_loop( ); } );
    }

    void ynison::stop( ) {
        running = false;
        if ( worker.joinable( ) )
            worker.join( );
    }

    bool ynison::get_redirect( ) {
        json proto = {
            {"Ynison-Device-Id", device_id},
            {"Ynison-Device-Info", "{\"app_name\":\"YM-Lib\",\"type\":1}"}
        };
        std::string protocol = "Bearer, v2, " + proto.dump( );

        ix::WebSocket ws;
        ws.setUrl( "wss://ynison.music.yandex.ru/redirector.YnisonRedirectService/GetRedirectToYnison" );

        ix::WebSocketHttpHeaders hdrs;
        hdrs[ "Sec-WebSocket-Protocol" ] = protocol;
        hdrs[ "Origin" ] = "http://music.yandex.ru";
        hdrs[ "Authorization" ] = "OAuth " + token;
        ws.setExtraHeaders( hdrs );

        bool ok = false;
        ws.setOnMessageCallback( [ & ]( const ix::WebSocketMessagePtr& msg ) {
            if ( msg->type == ix::WebSocketMessageType::Message ) {
                auto j = json::parse( msg->str );
                redirect_host = j.at( "host" ).get<std::string>( );
                redirect_ticket = j.at( "redirect_ticket" ).get<std::string>( );
                session_id = j.value( "session_id", "" );
                ok = true;
            }
            } );

        ws.start( );
        for ( int i = 0; i < 100 && !ok; i++ )
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        ws.stop( );
        return ok;
    }

    json ynison::build_initial_message( ) {
        long long ts = now_ms( );
        return {
            {"update_full_state", {
                {"player_state", {
                    {"player_queue", {
                        {"current_playable_index", -1},
                        {"entity_id", ""},
                        {"entity_type", "VARIOUS"},
                        {"playable_list", json::array( )},
                        {"options", {{"repeat_mode", "NONE"}}},
                        {"entity_context", "BASED_ON_ENTITY_BY_DEFAULT"},
                        {"version", {{"device_id", device_id}, {"version", ts}, {"timestamp_ms", ts}}}
                    }},
                    {"status", {
                        {"duration_ms", 0}, {"paused", true}, {"playback_speed", 1.0},
                        {"progress_ms", 0},
                        {"version", {{"device_id", device_id}, {"version", ts}, {"timestamp_ms", ts}}}
                    }}
                }},
                {"device", {
                    {"capabilities", {{"can_be_player", true}, {"can_be_remote_controller", false}, {"volume_granularity", 16}}},
                    {"info", {{"device_id", device_id}, {"type", "WEB"}, {"title", device_name}, {"app_name", "YM-Lib"}}},
                    {"volume_info", {{"volume", 50}}},
                    {"is_shadow", false}
                }},
                {"is_currently_active", true}
            }},
            {"rid", ynison_make_uuid( )},
            {"player_action_timestamp_ms", ts},
            {"activity_interception_type", "DO_NOT_INTERCEPT_BY_DEFAULT"}
        };
    }

    void ynison::handle_message( const std::string& raw ) {
        auto j = json::parse( raw );
        if ( !j.contains( "player_state" ) )
            return;

        auto& ps = j[ "player_state" ];
        ynison_state_t st;

        auto get_ll = [ ]( const json& j, const char* key ) -> long long {
            auto& v = j[ key ];
            if ( v.is_string( ) )
                return std::stoll( v.get<std::string>( ) );
            return v.get<long long>( );
            };
        auto get_int = [ ]( const json& j, const char* key ) -> int {
            auto& v = j[ key ];
            if ( v.is_string( ) )
                return std::stoi( v.get<std::string>( ) );
            return v.get<int>( );
            };

        st.progress_ms = get_ll( ps[ "status" ], "progress_ms" );
        st.duration_ms = get_ll( ps[ "status" ], "duration_ms" );
        st.paused = ps[ "status" ][ "paused" ].get<bool>( );
        st.playback_speed = ps[ "status" ][ "playback_speed" ].get<float>( );
        st.current_index = get_int( ps[ "player_queue" ], "current_playable_index" );

        auto& plist = ps[ "player_queue" ][ "playable_list" ];
        if ( st.current_index >= 0 && st.current_index < ( int )plist.size( ) )
            st.track_id = plist[ st.current_index ][ "playable_id" ].get<std::string>( );

        if ( on_state_changed )
            on_state_changed( st );
    }

    void ynison::connect_loop( ) {
        if ( !get_redirect( ) ) {
            std::cerr << "[ynison] redirect failed\n";
            return;
        }

        json proto = {
            {"Ynison-Device-Id", device_id},
            {"Ynison-Device-Info", "{\"app_name\":\"YM-Lib\",\"type\":1}"},
            {"Ynison-Redirect-Ticket", redirect_ticket}
        };
        std::string protocol = "Bearer, v2, " + proto.dump( );
        std::string url = "wss://" + redirect_host + "/ynison_state.YnisonStateService/PutYnisonState";

        ix::WebSocket ws;
        ws.setUrl( url );

        ix::WebSocketHttpHeaders hdrs;
        hdrs[ "Sec-WebSocket-Protocol" ] = protocol;
        hdrs[ "Origin" ] = "http://music.yandex.ru";
        hdrs[ "Authorization" ] = "OAuth " + token;
        ws.setExtraHeaders( hdrs );

        ws.setOnMessageCallback( [ & ]( const ix::WebSocketMessagePtr& msg ) {
            if ( msg->type == ix::WebSocketMessageType::Open )
                ws.sendText( build_initial_message( ).dump( ) );
            else if ( msg->type == ix::WebSocketMessageType::Message )
                handle_message( msg->str );
            else if ( msg->type == ix::WebSocketMessageType::Error )
                std::cerr << "[ynison] error: " << msg->errorInfo.reason << "\n";
            } );

        ws.start( );
        while ( running )
            std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
        ws.stop( );
    }

#endif // YM_WITH_YNISON

#ifdef YM_WITH_PLAYER
#include <SDL2/SDL.h>
#include <numeric>
#include <algorithm>

    player* player::s_instance = nullptr;

    player::player( ) {
        s_instance = this;
        if ( SDL_Init( SDL_INIT_AUDIO ) < 0 )
            return;
        int flags = MIX_INIT_MP3 | MIX_INIT_OGG;
        Mix_Init( flags );
        Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 2048 );
        Mix_HookMusicFinished( sdl_music_finished );
    }

    player::~player( ) {
        stop( );
        Mix_CloseAudio( );
        Mix_Quit( );
    }

    void player::set_queue( const std::vector<track_t>& tracks, int start_index ) {
        {
            std::lock_guard<std::mutex> lk( queue_mutex );
            queue_list = tracks;
            if ( shuffle ) {
                shuffle_order.resize( tracks.size( ) );
                std::iota( shuffle_order.begin( ), shuffle_order.end( ), 0 );
                auto it = std::find( shuffle_order.begin( ), shuffle_order.end( ), start_index );
                if ( it != shuffle_order.end( ) )
                    std::iter_swap( shuffle_order.begin( ), it );
                std::shuffle( shuffle_order.begin( ) + 1, shuffle_order.end( ), std::mt19937{ std::random_device{}( ) } );
            }
            cur_idx = -1;
        }
        load_and_play( start_index );
    }

    void player::add_to_queue( const track_t& t ) {
        int idx = -1;
        {
            std::lock_guard<std::mutex> lk( queue_mutex );
            queue_list.push_back( t );
            idx = ( int )queue_list.size( ) - 1;
            if ( shuffle )
                shuffle_order.push_back( idx );
        }
        if ( cur_idx < 0 )
            load_and_play( idx );
    }

    void player::clear_queue( ) {
        stop( );
        std::lock_guard<std::mutex> lk( queue_mutex );
        queue_list.clear( );
        shuffle_order.clear( );
        cur_idx = -1;
    }

    void player::load_and_play( int index ) {
        std::string id;
        {
            std::lock_guard<std::mutex> lk( queue_mutex );
            if ( index < 0 || index >= ( int )queue_list.size( ) )
                return;
            cur_idx = index;
            id = queue_list[ index ].id;
        }

        state = player_state_t::loading;
        if ( on_track_changed )
            on_track_changed( current_track( ) );
        if ( on_state_changed )
            on_state_changed( player_state_t::loading );

        int my_req = ++req_id;

        std::thread( [ this, id, my_req ]( ) {
            std::string url = on_stream_url ? on_stream_url( id ) : "";
            if ( url.empty( ) || my_req != req_id )
                return;

            std::vector<char> data;
            CURL* c = curl_easy_init( );
            if ( c ) {
                curl_easy_setopt( c, CURLOPT_URL, url.c_str( ) );
                curl_easy_setopt( c, CURLOPT_FOLLOWLOCATION, 1L );
                curl_easy_setopt( c, CURLOPT_WRITEFUNCTION, +[ ]( void* ptr, size_t s, size_t n, std::vector<char>* d ) {
                    d->insert( d->end( ), ( char* )ptr, ( char* )ptr + s * n );
                    return s * n;
                    } );
                curl_easy_setopt( c, CURLOPT_WRITEDATA, &data );
                curl_easy_perform( c );
                curl_easy_cleanup( c );
            }

            if ( data.empty( ) || my_req != req_id )
                return;

            std::lock_guard<std::mutex> lk( audio_mutex );
            if ( my_req != req_id )
                return;

            suppress_cb = true;
            Mix_HaltMusic( );
            if ( music ) {
                Mix_FreeMusic( music );
                music = nullptr;
            }

            audio_buf = std::move( data );
            SDL_RWops* rw = SDL_RWFromConstMem( audio_buf.data( ), ( int )audio_buf.size( ) );
            if ( !rw ) {
                state = player_state_t::stopped;
                suppress_cb = false;
                return;
            }

            music = Mix_LoadMUS_RW( rw, 1 );
            if ( music ) {
                cached_dur = ( float )Mix_MusicDuration( music );
                Mix_VolumeMusic( ( int )( volume * MIX_MAX_VOLUME ) );
                Mix_PlayMusic( music, 1 );
                state = player_state_t::playing;
                if ( on_state_changed )
                    on_state_changed( player_state_t::playing );
            }
            else {
                state = player_state_t::stopped;
            }
            suppress_cb = false;
            } ).detach( );
    }

    void player::play( ) {
        if ( cur_idx >= 0 )
            load_and_play( cur_idx );
    }

    void player::pause( ) {
        if ( state == player_state_t::playing ) {
            Mix_PauseMusic( );
            state = player_state_t::paused;
            if ( on_state_changed )
                on_state_changed( player_state_t::paused );
        }
    }

    void player::resume( ) {
        if ( state == player_state_t::paused ) {
            Mix_ResumeMusic( );
            state = player_state_t::playing;
            if ( on_state_changed )
                on_state_changed( player_state_t::playing );
        }
    }

    void player::stop( ) {
        ++req_id;
        suppress_cb = true;
        Mix_HaltMusic( );
        state = player_state_t::stopped;
        if ( on_state_changed )
            on_state_changed( player_state_t::stopped );
    }

    int player::next_idx( ) const {
        if ( queue_list.empty( ) )
            return -1;
        if ( shuffle && !shuffle_order.empty( ) ) {
            auto it = std::find( shuffle_order.begin( ), shuffle_order.end( ), cur_idx );
            if ( it == shuffle_order.end( ) )
                return -1;
            ++it;
            if ( it == shuffle_order.end( ) )
                return ( repeat == repeat_mode_t::all ) ? shuffle_order.front( ) : -1;
            return *it;
        }
        int next = cur_idx + 1;
        if ( next >= ( int )queue_list.size( ) )
            return ( repeat == repeat_mode_t::all ) ? 0 : -1;
        return next;
    }

    int player::prev_idx( ) const {
        if ( queue_list.empty( ) )
            return -1;
        if ( shuffle && !shuffle_order.empty( ) ) {
            auto it = std::find( shuffle_order.begin( ), shuffle_order.end( ), cur_idx );
            if ( it == shuffle_order.end( ) || it == shuffle_order.begin( ) )
                return -1;
            return *--it;
        }
        int prev = cur_idx - 1;
        if ( prev < 0 )
            return ( repeat == repeat_mode_t::all ) ? ( int )queue_list.size( ) - 1 : -1;
        return prev;
    }

    void player::next( ) {
        int i = -1;
        {
            std::lock_guard<std::mutex> lk( queue_mutex );
            i = next_idx( );
        }
        if ( i >= 0 )
            load_and_play( i );
        else
            stop( );
    }

    void player::prev( ) {
        if ( get_pos_sec( ) > 3.f ) {
            seek( 0.f );
            return;
        }
        int i = -1;
        {
            std::lock_guard<std::mutex> lk( queue_mutex );
            i = prev_idx( );
        }
        if ( i >= 0 )
            load_and_play( i );
    }

    void player::seek( float pct ) {
        std::lock_guard<std::mutex> lk( audio_mutex );
        if ( !music )
            return;
        float dur = ( float )Mix_MusicDuration( music );
        if ( dur > 0 )
            Mix_SetMusicPosition( ( double )( pct * dur ) );
    }

    void player::seek_sec( float sec ) {
        std::lock_guard<std::mutex> lk( audio_mutex );
        if ( music )
            Mix_SetMusicPosition( ( double )sec );
    }

    float player::get_progress( ) const {
        float d = get_dur_sec( );
        return d > 0 ? get_pos_sec( ) / d : 0.f;
    }

    float player::get_pos_sec( ) const {
        if ( state == player_state_t::stopped || state == player_state_t::loading )
            return 0.f;
        float p = ( float )Mix_GetMusicPosition( music );
        return p < 0.f ? 0.f : p;
    }

    const track_t* player::current_track( ) const {
        std::lock_guard<std::mutex> lk( queue_mutex );
        if ( cur_idx < 0 || cur_idx >= ( int )queue_list.size( ) )
            return nullptr;
        return &queue_list[ cur_idx ];
    }

    void player::set_volume( float v ) {
        volume = std::clamp( v, 0.f, 1.f );
        Mix_VolumeMusic( ( int )( volume * MIX_MAX_VOLUME ) );
    }

    void player::set_shuffle( bool s ) {
        shuffle = s;
        std::lock_guard<std::mutex> lk( queue_mutex );
        if ( s && !queue_list.empty( ) ) {
            shuffle_order.resize( queue_list.size( ) );
            std::iota( shuffle_order.begin( ), shuffle_order.end( ), 0 );
            auto it = std::find( shuffle_order.begin( ), shuffle_order.end( ), cur_idx );
            if ( it != shuffle_order.end( ) )
                std::iter_swap( shuffle_order.begin( ), it );
            std::shuffle( shuffle_order.begin( ) + 1, shuffle_order.end( ), std::mt19937{ std::random_device{}( ) } );
        }
        else {
            shuffle_order.clear( );
        }
    }

    void player::sdl_music_finished( ) {
        if ( !s_instance || s_instance->suppress_cb )
            return;
        if ( s_instance->repeat == repeat_mode_t::one ) {
            std::lock_guard<std::mutex> lk( s_instance->audio_mutex );
            if ( s_instance->music )
                Mix_PlayMusic( s_instance->music, 1 );
        }
        else {
            s_instance->next( );
        }
    }

#endif // YM_WITH_PLAYER

#if defined(YM_WITH_DISCORD) && defined(_WIN32)
#include <windows.h>
#include <cstring>
#include <cstdio>

    namespace ym {

        discord_rpc::discord_rpc( const std::string& app_id ) : app_id( app_id ) {}
        discord_rpc::~discord_rpc( ) {
            disconnect( );
        }

        bool discord_rpc::ipc_connect( ) {
            for ( int i = 0; i < 10; i++ ) {
                char name[ 64 ];
                sprintf_s( name, "\\\\.\\pipe\\discord-ipc-%d", i );
                HANDLE h = CreateFileA( name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr );
                if ( h != INVALID_HANDLE_VALUE ) {
                    pipe = h;
                    return true;
                }
            }
            return false;
        }

        void discord_rpc::ipc_disconnect( ) {
            if ( pipe && pipe != INVALID_HANDLE_VALUE )
                CloseHandle( ( HANDLE )pipe );
            pipe = nullptr;
            connected = false;
        }

        bool discord_rpc::ipc_write( const std::string& data, int opcode ) {
            if ( !pipe || pipe == INVALID_HANDLE_VALUE )
                return false;
            uint32_t op = ( uint32_t )opcode;
            uint32_t len = ( uint32_t )data.size( );
            uint8_t hdr[ 8 ];
            memcpy( hdr, &op, 4 );
            memcpy( hdr + 4, &len, 4 );
            DWORD w;
            return WriteFile( ( HANDLE )pipe, hdr, 8, &w, nullptr ) && w == 8
                && WriteFile( ( HANDLE )pipe, data.c_str( ), len, &w, nullptr ) && w == len;
        }

        std::string discord_rpc::ipc_read( ) {
            if ( !pipe || pipe == INVALID_HANDLE_VALUE )
                return {};
            uint8_t hdr[ 8 ];
            DWORD r;
            if ( !ReadFile( ( HANDLE )pipe, hdr, 8, &r, nullptr ) || r != 8 )
                return {};
            uint32_t len;
            memcpy( &len, hdr + 4, 4 );
            if ( !len || len > 65536 )
                return {};
            std::string buf( len, '\0' );
            ReadFile( ( HANDLE )pipe, &buf[ 0 ], len, &r, nullptr );
            return buf;
        }

        std::string discord_rpc::escape_json( const std::string& s ) {
            std::string out;
            for ( unsigned char c : s ) {
                switch ( c ) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   if ( c >= 32 ) out += ( char )c; break;
                }
            }
            return out;
        }

        std::string discord_rpc::build_handshake( ) {
            return "{\"v\":1,\"client_id\":\"" + app_id + "\"}";
        }

        void discord_rpc::connect( ) {
            if ( connected )
                return;
            if ( !ipc_connect( ) )
                return;
            if ( !ipc_write( build_handshake( ), 0 ) ) {
                ipc_disconnect( );
                return;
            }
            if ( ipc_read( ).empty( ) ) {
                ipc_disconnect( );
                return;
            }
            connected = true;
        }

        void discord_rpc::disconnect( ) {
            if ( connected ) {
                clear_activity( );
                ipc_disconnect( );
            }
        }

        void discord_rpc::set_activity( const std::string& details, const std::string& state, const std::string& large_image, int64_t start_ms, int64_t end_ms ) {
            if ( !connected )
                return;

            std::ostringstream ss;
            ss << "{\"cmd\":\"SET_ACTIVITY\","
                << "\"args\":{\"pid\":" << GetCurrentProcessId( ) << ","
                << "\"activity\":{"
                << "\"type\":2,"
                << "\"details\":\"" << escape_json( details ) << "\","
                << "\"state\":\"" << escape_json( state ) << "\"";

            if ( start_ms > 0 || end_ms > 0 ) {
                ss << ",\"timestamps\":{";
                if ( start_ms > 0 )
                    ss << "\"start\":" << start_ms;
                if ( start_ms > 0 && end_ms > 0 )
                    ss << ",";
                if ( end_ms > 0 )
                    ss << "\"end\":" << end_ms;
                ss << "}";
            }

            if ( !large_image.empty( ) ) {
                ss << ",\"assets\":{\"large_image\":\"" << escape_json( large_image ) << "\"}";
            }

            ss << "}},\"nonce\":\"" << ++nonce << "\"}";

            if ( !ipc_write( ss.str( ) ) ) {
                ipc_disconnect( );
                if ( ipc_connect( ) ) {
                    if ( ipc_write( build_handshake( ), 0 ) ) {
                        ipc_read( );
                        connected = true;
                        ipc_write( ss.str( ) );
                    }
                }
                return;
            }
            ipc_read( );
        }

        void discord_rpc::clear_activity( ) {
            if ( !connected )
                return;
            std::ostringstream ss;
            ss << "{\"cmd\":\"SET_ACTIVITY\","
                << "\"args\":{\"pid\":" << GetCurrentProcessId( ) << ",\"activity\":null},"
                << "\"nonce\":\"" << ++nonce << "\"}";
            ipc_write( ss.str( ) );
            ipc_read( );
        }

#ifdef YM_WITH_PLAYER
        void discord_rpc::update( player* p ) {
            if ( !connected || !p )
                return;

            const track_t* cur = p->current_track( );
            if ( !cur ) {
                if ( !last_track_id.empty( ) ) {
                    clear_activity( );
                    last_track_id.clear( );
                }
                return;
            }

            bool playing = ( p->get_state( ) == player_state_t::playing );
            bool track_changed = ( last_track_id != cur->id );
            bool pause_changed = ( last_paused != !playing );

            int64_t now = ( int64_t )std::time( nullptr );
            bool periodic = playing && ( now - last_update >= 15 );

            if ( !track_changed && !pause_changed && !periodic )
                return;

            last_track_id = cur->id;
            last_paused = !playing;
            last_update = now;

            std::string img = cur->cover( 400 );
            if ( img.empty( ) )
                img = "yandex_music_logo";

            int64_t start_ms = 0, end_ms = 0;
            if ( playing && p->get_dur_sec( ) > 0.f ) {
                int64_t now_ms = now * 1000LL;
                start_ms = now_ms - ( int64_t )( p->get_pos_sec( ) * 1000.f );
                end_ms = now_ms + ( int64_t )( ( p->get_dur_sec( ) - p->get_pos_sec( ) ) * 1000.f );
            }

            set_activity(
                cur->title.empty( ) ? "Unknown" : cur->title,
                "by " + ( cur->artist.empty( ) ? "Unknown" : cur->artist ),
                img,
                start_ms, end_ms
            );
        }
#endif // YM_WITH_PLAYER

    } // namespace ym
#endif // YM_WITH_DISCORD && _WIN32

} // namespace ym
#endif // YM_IMPLEMENTATION