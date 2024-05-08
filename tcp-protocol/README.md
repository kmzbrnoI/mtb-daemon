MTB daemon TCP JSON protocol description
========================================

MTB daemon features simple TCP server to control MTBbus. Daemon listens on port
specified in application's configuration. Usually, server listens only on
loopback interface (localhost) to filter unauthorised clients. This is
currently only way to assure authorization of clients.

Over the socket, data in json format in coding UTF-8 are sent in both
directions. Socket is kept open for the whole time of control software run,
because daemon server can send events to client. Data are divided into messages.
Each message in a single json dictionary on one line terminated with `\n`.

There are 3 types of messages:

 1. request
 2. response
 3. event (from server to client)

Client send requests, server responds to to requests with responses. Server
sends asynchronous events.

Each request can contain `id` key in root dictionary, type of the value must
be integer. Server copies this is to it's response. This mechanism allows client
to pair requests and responses. Client can not expect the first received message
to request will be response to the request. Any asynchronous event or response
to previous request can be received in the meantime.

### Common *request* attributes

 * `command`: string identifier of command
 * `type`: `request`
 * `id`: any number, will be sent in response
   - `id` could be omitted

### Common *response* attributes

 * `command`: identifier of response
 * `type`: `response`
 * `id`: request id for which response is generated
 * `status`: `ok` / `error`
   - In case of error, `error` dict is provided:
     `error: {"code": int, "message": str}`

### Common *event* attributes

 * `command`: string identifier of command
 * `type`: `event`

## Events

Server can send notification about module change to client. This change contains:

 * Module discovered
 * Module failed
 * Module inputs changed
 * Module outputs changed
 * Module configuration changed
 * Module state change (rebooting, firmware upgrading etc.)

To receive those events for a module client must send `module_subscribe`
request. Client can subscribe to module changes even if module does not exist
on bus currently.

When MTB-USB general change state (connected, disconnected, bus speed changed,
...) occurs, `mtbusb` event is sent to all connected clients.

## Addresses

Valid address of a MTB module: 1..255.

## [Messages specification](messages.md)

## Specialization of messages for module types

 * [MTB-UNI](uni.md)
