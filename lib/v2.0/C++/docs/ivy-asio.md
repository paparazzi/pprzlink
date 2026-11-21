# IvyLink with an application Asio context

Pass an application-owned Asio context to run SDK callbacks on that context.
Without a context, callbacks run on the native Ivy loop.

```cpp
boost::asio::io_context context;
pprzlink::IvyLink link(context, dictionary, "my application", domain);
link.bind("ALIVE", [](const pprzlink::Message &message) {
  // Runs on a thread servicing context.
});
link.start();
context.run(); // link.close() releases the link's work guard.
```

The context-first constructor owns an Ivy loop thread by default. Its optional
last argument, `threadedIvy=false`, lets an application retain control of the Ivy
loop and its error reporting. `getBus()` is a native API: its callbacks are
not redirected to Asio.

Receiver callbacks/predicates/error handlers, request reply callbacks and
request answerers execute on
the supplied context. They are serialized per link even when several threads
service that context. The SDK creates no Asio thread. SDK callback and reply
validation/send exceptions propagate to the caller of `context.run()` (or
`poll()`). Request parsing on the native loop still reports failures through Ivy;
Receiver decoding and its error notification run on Asio.

For a request, Ivy copies the decoded request and correlation ID into a queued
handler and returns immediately. The handler calls the answerer,
validates its answer schema, and sends the correlated reply using Ivy's
thread-safe send API. Multiple requests may be pending, each retaining its own
ID. Outgoing requests bind before sending and remove their one-shot reply
binding before posting the callback. Synchronous waits for an answer on the application context
can prevent its callbacks from progressing and must be avoided.

The dictionary and context must outlive the link. A shared lifetime gate stops
queued handlers from accessing the link after `close()` or destruction.
`close()` waits for an active SDK callback on another thread, suppresses queued
callbacks and replies, and releases the work guard. It is safe inside a callback;
a request answerer that closes the link does not send its returned answer.
Destruction cancels delivery before joining the owned Ivy thread. This works
even if the context was stopped with pending replies, since Ivy never waits for
Asio. Restarting the context later only drains inert handlers.

Ordinary messages use only the common Receiver `bind()`/`unbind(id)` API.
The link retains callbacks; discarding the returned ID does not unsubscribe.
Message and metadata references are borrowed for the call, so copy them before
retaining them. Receiver bindings use their own cancellation and start/stop
generation checks, suppressing queued delivery after removal or a pause.

The optional `RequestReply` interface uses `sendRequest()` / `cancelRequest()`
and `bindRequest()` / `unbindRequest()`, with receiver-owned integer IDs and
borrowed messages/optional metadata. Currently only Ivy implements this interface.
Cancellation suppresses already queued replies and answerer invocations; a callback
already executing may finish. Queued captures may remain allocated until the
context drains or is destroyed. `stop()` only pauses Receiver reception; it does
not close the bus or cancel request subscriptions/answerers. Finish external calls
and externally owned Ivy loops before destroying the link, and never destroy it
from a callback.
