# Mediator

**Intent:** define an object that encapsulates how a set of objects interact. It promotes loose coupling by keeping objects from referring to each other explicitly, and lets you vary their interaction independently.

Instead of every object knowing every other object, they all talk to **one coordinator**.

## The problem

A form has several widgets that affect each other: selecting "Other" in a dropdown enables a text box, checking "Subscribe" enables the email field, the Submit button is enabled only if everything is valid.

If widgets reference each other directly:

```text
Dropdown ◄──► TextBox ◄──► Checkbox
    ▲  ╲         ▲  ╲          ▲
    │    ╲       │    ╲        │
    └──────► SubmitButton ◄────┘
```

Every widget knows several others (up to N × (N-1) links), is hard to reuse alone, and any change ripples across all of them.

## Structure

```text
Colleague A ──┐
Colleague B ──┼──► Mediator ──► coordinates: tells A, B, C what to do
Colleague C ──┘
```

- **Mediator**: interface for communication with colleagues.
- **Concrete mediator**: knows all colleagues and holds the interaction logic.
- **Colleagues**: don't know each other, only the mediator. They notify it when something happens.

## Example: chat room

```java
public interface ChatMediator {
    void register(User user);
    void send(String message, User from);
    void sendPrivate(String message, User from, String toName);
}

public class ChatRoom implements ChatMediator {
    private final Map<String, User> users = new LinkedHashMap<>();

    @Override public void register(User user) {
        users.put(user.name(), user);
    }

    @Override public void send(String message, User from) {
        for (User u : users.values()) {
            if (u != from) u.receive(from.name() + ": " + message);
        }
    }

    @Override public void sendPrivate(String message, User from, String toName) {
        User target = users.get(toName);
        if (target != null) target.receive("(private) " + from.name() + ": " + message);
    }
}

public class User {
    private final String name;
    private final ChatMediator room;

    public User(String name, ChatMediator room) {
        this.name = name;
        this.room = room;
        room.register(this);
    }

    public String name() { return name; }
    public void say(String message) { room.send(message, this); }
    public void receive(String message) { System.out.println(name + " got: " + message); }
}

var room = new ChatRoom();
var alice = new User("Alice", room);
var bob   = new User("Bob", room);
alice.say("Hello");     // Bob receives it; Alice and Bob never reference each other
```

Adding rules (muting, moderation, blocking) changes `ChatRoom` only, and the users are untouched.

## Example: UI coordination

```java
public class SignupDialog implements DialogMediator {
    private final Checkbox subscribe = new Checkbox(this);
    private final TextField email = new TextField(this);
    private final Button submit = new Button(this);

    @Override public void changed(Component source) {
        if (source == subscribe) email.setEnabled(subscribe.isChecked());
        submit.setEnabled(!subscribe.isChecked() || email.isValidEmail());
    }
}
```

Each widget only calls `mediator.changed(this)`. The logic for how they relate lives in one place.

## Mediator vs Observer

| | Mediator | [Observer](./02-observer.md) |
|---|---|---|
| Shape | Many ↔ one ↔ many (two-way, coordinated) | One → many (one-way broadcast) |
| Who knows whom | Mediator knows all colleagues | Subject doesn't know observers |
| Logic | Interaction rules centralized | Each observer decides how to react |
| Purpose | Reduce tangled relationships | Notify of changes |

They're often combined: colleagues can notify the mediator through an observer-style event.

## Mediator vs Facade

A [Facade](../05-structural-patterns/05-facade.md) simplifies access to a subsystem in **one direction**, and the subsystem doesn't know about it. A mediator is **two-way**: colleagues actively talk to it and depend on it.

## Where you see it

- Dialog and form controllers.
- Chat rooms, message hubs.
- Air-traffic-control style coordination.
- Event buses and message routers inside an application.
- Application services coordinating several domain objects.
- MVC/MVP controllers that coordinate views and models.

## Benefits and costs

| Benefits | Costs |
|---|---|
| Colleagues are decoupled and easier to reuse | The mediator can become a **god object** with all the logic |
| Interaction logic is in one place | Complex mediators are hard to maintain |
| Easier to change interaction rules | Single point of coupling and failure |
| Replaces many-to-many with one-to-many | Colleagues depend on the mediator's interface |

## Common mistakes

- **A growing god mediator.** Split by concern (`FormValidationMediator`, `ChatModerationMediator`) or by feature.
- **Colleagues that still reference each other** alongside the mediator, undoing the decoupling.
- **Putting business logic in the mediator** that belongs in the colleagues' domain.
- **Using it for two objects**, where a direct call is simpler.
- **Circular notifications**: A tells the mediator, which updates B, which notifies the mediator, which updates A. Guard with change checks.

## Quick Summary

- Mediator centralizes how objects interact, so they no longer reference each other.
- It turns a tangle of many-to-many links into a star shape.
- The main risk is a bloated mediator, so keep it focused.
- Contrast: Observer is one-way broadcast, Mediator is coordinated two-way interaction.

## Related

[Observer](./02-observer.md) · [Facade](../05-structural-patterns/05-facade.md) · [Event-driven architecture](../08-architecture/05-event-driven.md)

## Next

[09-memento.md](./09-memento.md)
