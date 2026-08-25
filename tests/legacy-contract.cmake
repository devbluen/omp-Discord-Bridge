if(NOT DEFINED PROJECT_SOURCE_DIR OR NOT DEFINED PROJECT_BINARY_DIR)
	message(FATAL_ERROR "PROJECT_SOURCE_DIR and PROJECT_BINARY_DIR are required")
endif()

set(LEGACY_NAMES "${PROJECT_SOURCE_DIR}/tests/legacy-native-names.txt")
set(NEW_NATIVES "${PROJECT_SOURCE_DIR}/src/natives.cpp")
set(COMPONENT_SOURCE "${PROJECT_SOURCE_DIR}/src/discord-component.cpp")
set(BOT_SOURCE "${PROJECT_SOURCE_DIR}/src/discord-bot.cpp")
set(WEBSOCKET_SOURCE "${PROJECT_SOURCE_DIR}/src/discord-websocket.cpp")
set(COMPAT_INCLUDE "${PROJECT_BINARY_DIR}/pawno/include/discord-connector.inc")

file(READ "${LEGACY_NAMES}" OLD_TEXT)
file(READ "${NEW_NATIVES}" NEW_TEXT)
file(READ "${COMPONENT_SOURCE}" COMPONENT_TEXT)
file(READ "${BOT_SOURCE}" BOT_TEXT)
file(READ "${WEBSOCKET_SOURCE}" WEBSOCKET_TEXT)
file(READ "${COMPAT_INCLUDE}" INCLUDE_TEXT)

foreach(UNSUPPORTED_AMX_API amx_GetAddr amx_GetString amx_SetString amx_StrLen amx_PushAddress)
	string(FIND "${NEW_TEXT}" "${UNSUPPORTED_AMX_API}(" UNSUPPORTED_API_POSITION)
	if(NOT UNSUPPORTED_API_POSITION EQUAL -1)
		message(FATAL_ERROR
			"Native code must use open.mp IPawnScript wrappers instead of ${UNSUPPORTED_AMX_API}().")
	endif()
endforeach()

string(REPLACE "\n" ";" OLD_NAMES "${OLD_TEXT}")
list(FILTER OLD_NAMES EXCLUDE REGEX "^$")

string(REGEX MATCHALL "addNative\\(\"DCC_[A-Za-z0-9_]+\"\\)" NEW_MATCHES "${NEW_TEXT}")
set(NEW_NAMES)
foreach(MATCH IN LISTS NEW_MATCHES)
	string(REGEX REPLACE ".*addNative\\(\"([A-Za-z0-9_]+)\"\\).*" "\\1" NAME "${MATCH}")
	list(APPEND NEW_NAMES "${NAME}")
endforeach()

list(REMOVE_DUPLICATES OLD_NAMES)
list(REMOVE_DUPLICATES NEW_NAMES)
list(FILTER OLD_NAMES EXCLUDE REGEX "^native_name$")
list(SORT OLD_NAMES)
list(SORT NEW_NAMES)
if(NOT OLD_NAMES STREQUAL NEW_NAMES)
	message(FATAL_ERROR "Legacy native contract mismatch. Old=${OLD_NAMES} New=${NEW_NAMES}")
endif()

foreach(REQUIRED
	"DCC_FindChannelByName"
	"DCC_FindGuildByName"
	"DCC_CacheChannelMessage"
	"DCC_OnMessageReaction")
	string(FIND "${INCLUDE_TEXT}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Compatibility include is missing '${REQUIRED}'")
	endif()
endforeach()

string(FIND "${COMPONENT_TEXT}" "getKnownGuildIds" GUILDS_POSITION)
string(FIND "${COMPONENT_TEXT}" "Name lookup is deliberately cache-only" CACHE_POSITION)
string(FIND "${BOT_TEXT}" "MAX_EVENTS_PER_TICK" BUDGET_POSITION)
string(FIND "${BOT_TEXT}" "connectThread_" CONNECT_THREAD_POSITION)
string(FIND "${WEBSOCKET_TEXT}" "erase(\"members\")" SNAPSHOT_POSITION)
string(FIND "${NEW_TEXT}" "g_createdMessageHandle = assignMessageHandle" CREATED_HANDLE_POSITION)
string(FIND "${NEW_TEXT}" "executePawnCallback(*callback)" CALLBACK_POSITION)
string(FIND "${NEW_TEXT}" "bridge->removeMessage(createdMessageId)" CALLBACK_CLEANUP_POSITION)
if(GUILDS_POSITION EQUAL -1 OR CACHE_POSITION EQUAL -1 OR BUDGET_POSITION EQUAL -1 OR CONNECT_THREAD_POSITION EQUAL -1 OR SNAPSHOT_POSITION EQUAL -1)
	message(FATAL_ERROR "Dynamic guild/channel discovery is not wired into the gateway cache")
endif()

if(CREATED_HANDLE_POSITION EQUAL -1 OR CALLBACK_POSITION EQUAL -1 OR CALLBACK_CLEANUP_POSITION EQUAL -1 OR CALLBACK_POSITION GREATER CALLBACK_CLEANUP_POSITION)
	message(FATAL_ERROR "Created-message callbacks must run before temporary message cleanup")
endif()

message(STATUS "Legacy native/include and lazy dynamic discovery contract passed")
