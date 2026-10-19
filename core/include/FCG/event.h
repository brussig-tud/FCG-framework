
#ifndef __FCG_EVENT_H__
#define __FCG_EVENT_H__


//////
//
// Module documentation
//

/// \defgroup fcg_events Events
/// \ingroup fcg_modules
/// \brief Input events of the main window, delivered to the applets' \c onEvent callbacks.
///
/// Pending keyboard, mouse and drag-and-drop events of the main window are delivered through
/// <tt>\ref fcg::Applet::onEvent "Applet::onEvent"</tt> during each main-loop iteration. An
/// <tt>\ref fcg::Event "Event"</tt> carries an <tt>\ref fcg::EventType "EventType"</tt> and, for most event kinds, a
/// typed payload such as <tt>\ref fcg::KeyEvent "KeyEvent"</tt> or <tt>\ref fcg::MouseButtonEvent
/// "MouseButtonEvent"</tt>. An <tt>\ref fcg::EventContext "EventContext"</tt> lets applets consume events.
///
/// \section fcg_events_usage Getting started
/// Override <tt>\ref fcg::Applet::onEvent "Applet::onEvent"</tt>, switch over the event type, and access the typed
/// payload:
/// \snippet event_examples.cpp event_handling
///
/// \section fcg_events_workflows Common tasks
/// \subsection fcg_events_workflows_payloads Event kinds and payloads
/// The <tt>\ref fcg::EventType "EventType"</tt> enumeration lists all event kinds together with the payload type each
/// one carries: key events (<tt>\ref fcg::EventType::KeyDown "KeyDown"</tt>,
/// <tt>\ref fcg::EventType::KeyUp "KeyUp"</tt>), text input, mouse motion, button and wheel events, and drag-and-drop
/// of files and text. Within these delivered categories, events without a framework equivalent arrive as
/// <tt>\ref fcg::EventType::Raw "Raw"</tt>, with the SDL event accessible through
/// <tt>\ref fcg::Event::raw "Event::raw"</tt>. \b Other SDL event categories, including touch, pen and user events,
/// are <b>not currently delivered</b> to applets.
///
/// \subsection fcg_events_workflows_coordinates Coordinates
/// Mouse positions are given in window coordinates relative to the top-left corner of the window. On high-DPI displays
/// these can differ from the pixel coordinates of the viewport; <tt>\ref fcg::Player::viewportSize
/// "Player::viewportSize"</tt> returns the pixel size.
///
/// \subsection fcg_events_workflows_handling Cooperation between applets
/// Events are delivered in the order in which applets were passed to <tt>\ref fcg::run "run"</tt>. Call
/// <tt>\ref fcg::EventContext::markHandled "EventContext::markHandled"</tt> when consuming an event. Once that
/// callback returns, delivery stops: remaining applets do not receive the event. Marking an Escape
/// <tt>\ref fcg::EventType::KeyDown "KeyDown"</tt> event as handled also suppresses the player's close action.
/// Events that the GUI captures (e.g. keyboard input while a text field is focused) are not delivered to applets;
/// see \ref fcg_gui.
///
/// \section fcg_events_lifetime Ownership and lifetime
/// The <tt>\ref fcg::Event "Event"</tt> passed to <tt>\ref fcg::Applet::onEvent "onEvent"</tt> is only valid during the
/// call, and the same holds for the payload pointer obtained from it. Copy the data you need; do not store the event
/// or pointers obtained from it.
///
/// \see \ref fcg_applets, \ref fcg_runtime, \ref fcg_gui



//////
//
// Includes
//

// C++ STL
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <utility>

// Local includes
#include "FCG/export.h"



//////
//
// Forward declarations
//

// Opaque SDL3 types
union SDL_Event;



//////
//
// Namespaces open
//

/// The library top-level namespace.
namespace fcg {

/// \addtogroup fcg_events
/// @{



//////
//
// Structs and enums
//

/// The type of an <tt>\ref fcg::Event "Event"</tt>, which also determines the type of its payload.
enum class EventType
{
	/// Any other SDL event. Carries no payload; inspect <tt>\ref fcg::Event::raw "Event::raw"</tt> instead.
	Raw,

	/// A key was pressed, or is held down and repeats. Carries a <tt>\ref fcg::KeyEvent "KeyEvent"</tt>.
	KeyDown,

	/// A key was released. Carries a <tt>\ref fcg::KeyEvent "KeyEvent"</tt>.
	KeyUp,

	/// Text was entered. Carries a <tt>\ref fcg::TextEvent "TextEvent"</tt>.
	TextInput,

	/// Text is being composed by an input method. Carries a <tt>\ref fcg::TextEvent "TextEvent"</tt>.
	TextEditing,

	/// The mouse cursor moved. Carries a <tt>\ref fcg::MouseMotionEvent "MouseMotionEvent"</tt>.
	MouseMotion,

	/// A mouse button was pressed. Carries a <tt>\ref fcg::MouseButtonEvent "MouseButtonEvent"</tt>.
	MouseButtonDown,

	/// A mouse button was released. Carries a <tt>\ref fcg::MouseButtonEvent "MouseButtonEvent"</tt>.
	MouseButtonUp,

	/// The mouse wheel was scrolled. Carries a <tt>\ref fcg::MouseWheelEvent "MouseWheelEvent"</tt>.
	MouseWheel,

	/// A drag-and-drop operation onto the window began. Carries a <tt>\ref fcg::DropEvent "DropEvent"</tt>
	/// without position or data.
	DropBegin,

	/// Dragged data was moved over the window. Carries a <tt>\ref fcg::DropEvent "DropEvent"</tt> with the cursor
	/// position and empty data.
	DropPosition,

	/// A file was dropped onto the window. Carries a <tt>\ref fcg::DropEvent "DropEvent"</tt> with the file path.
	DropFile,

	/// Text was dropped onto the window. Carries a <tt>\ref fcg::DropEvent "DropEvent"</tt> with the text.
	DropText,

	/// A drag-and-drop operation onto the window finished. Carries a <tt>\ref fcg::DropEvent "DropEvent"</tt>
	/// without position or data.
	DropComplete
};

/// Mouse buttons recognized by the framework. The values correspond to the SDL button indices.
enum class MouseButton : std::uint8_t
{
	/// No recognized mouse button.
	Unknown,

	/// The left mouse button.
	Left,

	/// The middle mouse button.
	Middle,

	/// The right mouse button.
	Right,

	/// The first extra mouse button (typically browser back).
	X1,

	/// The second extra mouse button (typically browser forward).
	X2
};

/// The payload of <tt>\ref fcg::EventType::KeyDown "KeyDown"</tt> and <tt>\ref fcg::EventType::KeyUp "KeyUp"</tt>
/// events.
struct KeyEvent
{
	/// The virtual key code, as an \c SDL_Keycode (e.g. \c SDLK_SPACE). Depends on the keyboard layout.
	std::uint32_t key = 0;

	/// The physical key, as an \c SDL_Scancode (e.g. \c SDL_SCANCODE_W). Independent of the keyboard layout.
	std::uint32_t scancode = 0;

	/// The key modifiers active at the time of the event, as an \c SDL_Keymod bitmask (e.g. \c SDL_KMOD_SHIFT).
	std::uint32_t modifiers = 0;

	/// Whether this key press is an automatic repetition from holding the key down. Always \c false for key releases.
	bool repeat = false;
};

/// The payload of <tt>\ref fcg::EventType::TextInput "TextInput"</tt> and
/// <tt>\ref fcg::EventType::TextEditing "TextEditing"</tt> events.
struct TextEvent
{
	/// The entered or edited text, UTF-8 encoded.
	std::string text;
};

/// \brief
/// 	The payload of <tt>\ref fcg::EventType::MouseMotion "MouseMotion"</tt> events.
///
/// Positions and movements are given in SDL window coordinates, relative to the top-left corner of the window. On
/// high-DPI displays, these can differ from the pixel coordinates of the viewport.
struct MouseMotionEvent
{
	/// The horizontal cursor position.
	float x = 0;

	/// The vertical cursor position, growing downwards.
	float y = 0;

	/// The horizontal movement since the previous motion event.
	float relativeX = 0;

	/// The vertical movement since the previous motion event.
	float relativeY = 0;

	/// The mouse buttons held down during the motion, as an \c SDL_MouseButtonFlags bitmask. Test individual buttons
	/// with \c SDL_BUTTON_MASK, e.g. <tt>buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)</tt>.
	std::uint32_t buttons = 0;
};

/// The payload of <tt>\ref fcg::EventType::MouseButtonDown "MouseButtonDown"</tt> and
/// <tt>\ref fcg::EventType::MouseButtonUp "MouseButtonUp"</tt> events. Positions are given in SDL window coordinates
/// as for <tt>\ref fcg::MouseMotionEvent "MouseMotionEvent"</tt>.
struct MouseButtonEvent
{
	/// The horizontal cursor position at the time of the event.
	float x = 0;

	/// The vertical cursor position at the time of the event, growing downwards.
	float y = 0;

	/// The mouse button that changed state.
	MouseButton button = MouseButton::Unknown;

	/// The number of consecutive clicks as reported by SDL: 1 for a single click, 2 for a double click, and so on.
	std::uint8_t clicks = 0;
};

/// The payload of <tt>\ref fcg::EventType::MouseWheel "MouseWheel"</tt> events.
struct MouseWheelEvent
{
	/// The amount scrolled horizontally as reported by SDL, positive to the right.
	float x = 0;

	/// The amount scrolled vertically as reported by SDL, positive away from the user.
	float y = 0;
};

/// \brief
/// 	The payload of the drag-and-drop events <tt>\ref fcg::EventType::DropBegin "DropBegin"</tt>,
/// 	<tt>\ref fcg::EventType::DropPosition "DropPosition"</tt>, <tt>\ref fcg::EventType::DropFile "DropFile"</tt>,
/// 	<tt>\ref fcg::EventType::DropText "DropText"</tt> and <tt>\ref fcg::EventType::DropComplete "DropComplete"</tt>.
///
/// Positions are given in SDL window coordinates as for <tt>\ref fcg::MouseMotionEvent "MouseMotionEvent"</tt>.
struct DropEvent
{
	/// The horizontal cursor position at the time of the event. 0 for \c DropBegin and \c DropComplete.
	float x = 0;

	/// The vertical cursor position at the time of the event. 0 for \c DropBegin and \c DropComplete.
	float y = 0;

	/// The file path for \c DropFile, or the text for \c DropText. Empty for all other drop events.
	std::string data;
};

/// The payload of an <tt>\ref fcg::Event "Event"</tt>. Which alternative is held depends on the
/// <tt>\ref fcg::EventType "EventType"</tt>; \c std::monostate means no payload.
using EventPayload = std::variant<
	std::monostate, KeyEvent, TextEvent, MouseMotionEvent, MouseButtonEvent, MouseWheelEvent, DropEvent
>;



//////
//
// Classes
//

/// \brief
/// 	Context of one event, shared by all applets that receive it.
///
/// Call \c markHandled when consuming an event. See \ref fcg_events_workflows_handling for the dispatch rules.
class FCG_FRAMEWORK_EXPORT EventContext
{
public:

	////
	// Accessors

	/// \brief
	/// 	Whether an applet has already marked this event as handled.
	///
	/// \return \c true if <tt>\ref markHandled "markHandled"</tt> has been called for this event.
	[[nodiscard]] inline auto wasHandled () const -> bool {
		return handled;
	}


	////
	// Methods

	/// Marks this event as consumed; see \ref fcg_events_workflows_handling for its effect on dispatch.
	void markHandled () {
		handled = true;
	}


private:

	////
	// Fields

	/// Whether the event has been handled by an applet.
	bool handled = false;
};

/// \brief
/// 	An input event of the main window, as passed to <tt>\ref fcg::Applet::onEvent "Applet::onEvent"</tt>.
///
/// Query the kind of event with <tt>\ref type "type"</tt> and retrieve its payload with <tt>\ref data "data"</tt>,
/// for example:
/// \snippet event_examples.cpp key_payload
///
/// An event refers to the raw SDL event it was made from. The events passed to \c Applet::onEvent are therefore only
/// valid during that call: do not store them, or pointers or references obtained from them, but copy the data you
/// need.
class FCG_FRAMEWORK_EXPORT Event
{
public:

	////
	// Object construction/destruction

	/// \brief
	/// 	Constructs an event from its type, the raw SDL event and the payload. Called by
	/// 	<tt>\ref fcg::makeEvent "makeEvent"</tt>; applications normally don't call this.
	///
	/// \pre
	/// 	\p raw is not \c nullptr and stays valid as long as the event is used.
	///
	/// \param type The type of the event.
	/// \param raw The SDL event this event refers to. Not owned.
	/// \param payload The payload matching \p type, as listed for <tt>\ref fcg::EventType "EventType"</tt>.
	Event (EventType type, const SDL_Event *raw, EventPayload payload={})
		: m_type(type), m_raw(raw), m_payload(std::move(payload))
	{}


	////
	// Accessors

	/// The type of this event.
	[[nodiscard]] inline auto type () const -> EventType {
		return m_type;
	}

	/// \brief
	/// 	The raw SDL event this event was made from. Use it for events of type \c EventType::Raw or for details not
	/// 	covered by the payload.
	///
	/// \return A reference to the SDL event, valid as long as this event is.
	[[nodiscard]] auto raw () const -> const SDL_Event& {
		return *m_raw;
	}

	/// \brief
	/// 	Accesses the payload of this event.
	///
	/// \tparam T The payload type to retrieve, e.g. <tt>\ref fcg::KeyEvent "KeyEvent"</tt>.
	///
	/// \return
	/// 	A pointer to the payload if this event carries a payload of type \c T, or \c nullptr otherwise. The pointer
	/// 	is valid as long as this event is.
	template<class T>
	[[nodiscard]] inline auto data () const -> const T* {
		return std::get_if<T>(&m_payload);
	}


private:

	////
	// Fields

	/// The normalized type of this event.
	EventType m_type;

	/// The raw SDL event that this event wraps.
	const SDL_Event *m_raw;

	/// The typed payload, if any.
	EventPayload m_payload;
};



//////
//
// Functions
//

/// \brief
/// 	Converts an SDL event into a framework <tt>\ref fcg::Event "Event"</tt> with the matching
/// 	<tt>\ref fcg::EventType "EventType"</tt> and payload. Called by \c fcg::run for every event delivered to the
/// 	applets; applications normally don't call this.
///
/// SDL events without a framework equivalent become events of type \c EventType::Raw.
///
/// \param event The SDL event to convert. The returned event refers to it, so it must outlive the returned event.
///
/// \return The framework event.
FCG_FRAMEWORK_EXPORT auto makeEvent (const SDL_Event &event) -> Event;



//////
//
// Namespaces close
//

/// @}

// namespace fcg
}


#endif  // ifndef __FCG_EVENT_H__
