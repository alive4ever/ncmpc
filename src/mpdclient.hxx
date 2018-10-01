#ifndef MPDCLIENT_H
#define MPDCLIENT_H

#include "config.h"
#include "Queue.hxx"
#include "gidle.hxx"
#include "util/Compiler.h"
#include "AsioServiceFwd.hxx"

#ifdef ENABLE_ASYNC_CONNECT
#include "aconnect.hxx"
#endif

#include <mpd/client.h>

#include <boost/asio/steady_timer.hpp>

#include <string>

struct AsyncMpdConnect;
struct MpdQueue;
class MpdIdleSource;
class FileList;

struct mpdclient final
	: MpdIdleHandler
#ifdef ENABLE_ASYNC_CONNECT
	, AsyncMpdConnectHandler
#endif
{
#ifdef ENABLE_ASYNC_CONNECT
	/**
	 * These settings are used to connect to MPD asynchronously.
	 */
	struct mpd_settings *settings;

#ifndef _WIN32
	/**
	 * A second set of settings, just in case #settings did not
	 * work.  This is only used if #settings refers to a local
	 * socket path, and this one is supposed to be a fallback to
	 * IP on the default port (6600).
	 */
	struct mpd_settings *settings2;
#endif

#else
	const char *host;
	unsigned port;
#endif

	const unsigned timeout_ms;

	const char *const password;

	/* playlist */
	MpdQueue playlist;

#ifdef ENABLE_ASYNC_CONNECT
	AsyncMpdConnect *async_connect = nullptr;
#endif

	struct mpd_connection *connection = nullptr;

	/**
	 * Tracks idle events.  It is automatically called by
	 * mpdclient_get_connection().
	 */
	MpdIdleSource *source = nullptr;

	struct mpd_status *status = nullptr;
	const struct mpd_song *song = nullptr;

	/**
	 * A timer which re-enters MPD idle mode before the next main
	 * loop iteration.
	 */
	boost::asio::steady_timer enter_idle_timer;

	/**
	 * This attribute is incremented whenever the connection changes
	 * (i.e. on disconnection and (re-)connection).
	 */
	unsigned connection_id = 0;

	int volume = -1;

	/**
	 * A bit mask of idle events occurred since the last update.
	 */
	unsigned events = 0;

	enum mpd_state state = MPD_STATE_UNKNOWN;

#if defined(ENABLE_ASYNC_CONNECT) && !defined(_WIN32)
	bool connecting2;
#endif

	/**
	 * This attribute is true when the connection is currently in
	 * "idle" mode, and the #mpd_glib_source waits for an event.
	 */
	bool idle = false;

	/**
	 * Is MPD currently playing?
	 */
	bool playing = false;

	/**
	 * Is MPD currently playing or paused?
	 */
	bool playing_or_paused = false;

	mpdclient(boost::asio::io_service &io_service,
		  const char *host, unsigned port,
		  unsigned _timeout_ms, const char *_password);

	~mpdclient() {
		Disconnect();

#ifdef ENABLE_ASYNC_CONNECT
		mpd_settings_free(settings);

#ifndef _WIN32
		if (settings2 != nullptr)
			mpd_settings_free(settings2);
#endif
#endif
	}

	auto &get_io_service() noexcept {
		return enter_idle_timer.get_io_service();
	}

	/**
	 * Determine a human-readable "name" of the settings currently used to
	 * connect to MPD.
	 *
	 * @return an allocated string that needs to be freed (with g_free())
	 * by the caller
	 */
	std::string GetSettingsName() const;

	bool IsConnected() const {
		return connection != nullptr;
	}

	/**
	 * Is this object "dead"?  i.e. not connected and not
	 * currently doing anything to connect.
	 */
	gcc_pure
	bool IsDead() const {
		return connection == nullptr
#ifdef ENABLE_ASYNC_CONNECT
			&& async_connect == nullptr
#endif
			;
	}

	gcc_pure
	int GetCurrentSongId() const noexcept {
		return status != nullptr
			? mpd_status_get_song_id(status)
			: -1;
	}

	/**
	 * Returns the song that is "current".  This can be valid even
	 * if MPD is not playing.
	 */
	gcc_pure
	const struct mpd_song *GetCurrentSong() const {
		return song;
	}

	/**
	 * Returns the song that is currently being played (or
	 * paused).
	 */
	gcc_pure
	const struct mpd_song *GetPlayingSong() const {
		return playing_or_paused
			? GetCurrentSong()
			: nullptr;
	}

	void Connect();

	void Disconnect();

	bool HandleError();

	struct mpd_connection *GetConnection();

	bool FinishCommand() {
		return mpd_response_finish(connection) || HandleError();
	}

	bool Update();

	bool OnConnected(struct mpd_connection *_connection) noexcept;

private:
	bool UpdateQueue();
	bool UpdateQueueChanges();

	void ClearStatus() noexcept;

	void ScheduleEnterIdle() noexcept;
	void CancelEnterIdle() noexcept {
		enter_idle_timer.cancel();
	}
	void OnEnterIdleTimer(const boost::system::error_code &error) noexcept;

#ifdef ENABLE_ASYNC_CONNECT
	/* virtual methods from AsyncMpdConnectHandler */
	void OnAsyncMpdConnect(struct mpd_connection *c) noexcept override;
	void OnAsyncMpdConnectError(const char *message) noexcept override;
#endif

	/* virtual methods from MpdIdleHandler */
	void OnIdle(unsigned events) noexcept override;
	void OnIdleError(enum mpd_error error,
			 enum mpd_server_error server_error,
			 const char *message) noexcept override;
};

enum {
	/**
	 * all idle events the version of libmpdclient, ncmpc is compiled
	 * against, supports
	 */
	MPD_IDLE_ALL = MPD_IDLE_DATABASE
		| MPD_IDLE_STORED_PLAYLIST
		| MPD_IDLE_QUEUE
		| MPD_IDLE_PLAYER
		| MPD_IDLE_MIXER
		| MPD_IDLE_OUTPUT
		| MPD_IDLE_OPTIONS
		| MPD_IDLE_UPDATE
		| MPD_IDLE_STICKER
		| MPD_IDLE_SUBSCRIPTION
		| MPD_IDLE_MESSAGE
};

/*** MPD Commands  **********************************************************/

bool
mpdclient_cmd_crop(struct mpdclient *c);

bool
mpdclient_cmd_clear(struct mpdclient *c);

bool
mpdclient_cmd_volume(struct mpdclient *c, int value);

bool
mpdclient_cmd_volume_up(struct mpdclient *c);

bool
mpdclient_cmd_volume_down(struct mpdclient *c);

bool
mpdclient_cmd_add_path(struct mpdclient *c, const char *path);

bool
mpdclient_cmd_add(struct mpdclient *c, const struct mpd_song *song);

bool
mpdclient_cmd_delete(struct mpdclient *c, int index);

bool
mpdclient_cmd_delete_range(struct mpdclient *c, unsigned start, unsigned end);

bool
mpdclient_cmd_move(struct mpdclient *c, unsigned dest, unsigned src);

bool
mpdclient_cmd_subscribe(struct mpdclient *c, const char *channel);

bool
mpdclient_cmd_unsubscribe(struct mpdclient *c, const char *channel);

bool
mpdclient_cmd_send_message(struct mpdclient *c, const char *channel,
			   const char *text);

#endif
