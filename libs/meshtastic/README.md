# Meshtastic

## Code Gen

Meshtastic client API is defined as a [Protobuf schema](https://buf.build/meshtastic/protobufs), the schema is published to the Buf Schema Registry.

[Buf](https://buf.build) is used to pull in the remote Meshtastic schema and then embedded C code is generated using [nanopb](https://github.com/nanopb/nanopb), which conveniently is also served up via a [Buf plugin](https://buf.build/formal/core/sdks/main:community/nanopb).
