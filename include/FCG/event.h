
#ifndef __FCG_EVENT_H__
#define __FCG_EVENT_H__


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



//////
//
// Enums
//

/// The type of an \ref Event.
enum class EventType
{
	/// An unrecognized or unhandled SDL event.
	Raw,

	/// A key was pressed.
	KeyDown,

	/// A key was released.
	KeyUp,

	/// Text input was entered.
	TextInput,

	/// Text editing occurred.
	TextEditing,

	/// The mouse cursor moved.
	MouseMotion,

	/// A mouse button was pressed.
	MouseButtonDown,

	/// A mouse button was released.
	MouseButtonUp,

	/// The mouse wheel was scrolled.
	MouseWheel,

	/// A drag-and-drop operation began.
	DropBegin,

	/// A drag-and-drop item was moved over the window.
	DropPosition,

	/// A file was dropped onto the window.
	DropFile,

	/// Text was dropped onto the window.
	DropText,

	/// A drag-and-drop operation finished.
	DropComplete
};

/// Mouse buttons recognized by the framework.
enum class MouseButton : std::uint8_t
{
	/// An unknown or unsupported mouse button.
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



//////
//
// Structs
//

/// Data associated with a keyboard event.
struct KeyEvent
{
	/// The SDL key code.
	std::uint32_t key = 0;

	/// The SDL scan code.
	std::uint32_t scancode = 0;

	/// Bitmask of SDL key modifiers active at the time of the event.
	std::uint32_t modifiers = 0;

	/// Whether this is a repeated key event from holding the key down.
	bool repeat = false;
};

/// Data associated with a text input or editing event.
struct TextEvent {
	/// The entered or edited text.
	std::string text;
};

/// Data associated with a mouse motion event.
struct MouseMotionEvent
{
	/// The cursor x position, in window coordinates.
	float x = 0;

	/// The cursor y position, in window coordinates.
	float y = 0;

	/// The relative x movement since the last motion event.
	float relativeX = 0;

	/// The relative y movement since the last motion event.
	float relativeY = 0;

	/// Bitmask of mouse buttons held down during the motion.
	std::uint32_t buttons = 0;
};

/// Data associated with a mouse button event.
struct MouseButtonEvent
{
	/// The cursor x position at the time of the click, in window coordinates.
	float x = 0;

	/// The cursor y position at the time of the click, in window coordinates.
	float y = 0;

	/// The mouse button that changed state.
	MouseButton button = MouseButton::Unknown;

	/// The number of clicks associated with the event (1 for single click, 2 for double click).
	std::uint8_t clicks = 0;
};

/// Data associated with a mouse wheel event.
struct MouseWheelEvent {
	/// The horizontal scroll amount.
	float x = 0;

	/// The vertical scroll amount.
	float y = 0;
};

/// Data associated with a drag-and-drop event.
struct DropEvent
{
	/// The cursor x position at the time of the event, in window coordinates.
	float x = 0;

	/// The cursor y position at the time of the event, in window coordinates.
	float y = 0;

	/// The dropped data, e.g. a file path or text. Empty for events that carry no data.
	std::string data;
};



//////
//
// Type aliases
//

/// The discriminated payload carried by an \ref Event.
using EventPayload = std::variant<
	std::monostate, KeyEvent, TextEvent, MouseMotionEvent, MouseButtonEvent, MouseWheelEvent, DropEvent
>;



//////
//
// Classes
//

/// Per-event context shared between every active \ref fcg::Applet.
///
/// Applets can use this object to report that they have consumed an event, so that subsequent applets can decide
/// whether they still want to process it.
class FCG_FRAMEWORK_EXPORT EventContext
{
	////
	// Friend declarations

	/// The dispatcher may reset the handled state when reusing context objects internally.
	friend class EventDispatcher;


public:

	////
	// Accessors

	/// Whether another applet has already handled this event.
	[[nodiscard]] inline auto wasHandled () const -> bool {
		return handled;
	}


	////
	// Methods

	/// Mark this event as handled by the current applet.
	void markHandled () {
		handled = true;
	}


private:

	////
	// Fields

	/// Whether the event has been handled by an applet.
	bool handled = false;
};

/// An event that an \ref fcg::Applet can react to.
class FCG_FRAMEWORK_EXPORT Event
{
public:

	////
	// Object construction/destruction

	/// Construct an event from its type, the raw SDL event and an optional typed payload.
	Event (EventType type, const SDL_Event *raw, EventPayload payload={})
		: m_type(type), m_raw(raw), m_payload(std::move(payload))
	{}


	////
	// Accessors

	/// The normalized type of this event.
	[[nodiscard]] inline auto type () const -> EventType {
		return m_type;
	}

	/// The raw SDL event that this event wraps.
	[[nodiscard]] auto raw () const -> const SDL_Event& {
		return *m_raw;
	}

	/// Access the typed payload of this event.
	///
	/// \tparam T The payload type to retrieve, e.g. \ref KeyEvent.
	///
	/// \returns A pointer to the payload if this event carries a payload of type \c T, or `nullptr` otherwise.
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

/// Convert a raw SDL event into a normalized framework \ref Event.
///
/// \param event The SDL event to convert.
///
/// \returns The normalized \ref Event carrying the appropriate \ref EventType and payload.
FCG_FRAMEWORK_EXPORT auto makeEvent (const SDL_Event &event) -> Event;



//////
//
// Namespaces close
//

// namespace fcg
}


#endif  // ifndef __FCG_EVENT_H__
