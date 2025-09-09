#include <jswrap_io.h>
#include <pb_encode.h>
#include <pb_decode.h>

#include "gen/meshtastic/config.pb.h"
#include "gen/meshtastic/mesh.pb.h"

bool encode_unionmessage(pb_ostream_t *stream, const pb_msgdesc_t *messagetype, void *message)
{
  pb_field_iter_t iter;

  if (!pb_field_iter_begin(&iter, meshtastic_ToRadio_fields, message))
    return false;

  do
  {
    if (iter.submsg_desc == messagetype)
    {
      /* This is our field, encode the message using it. */
      if (!pb_encode_tag_for_field(stream, &iter))
        return false;

      return pb_encode_submessage(stream, messagetype, message);
    }
  } while (pb_field_iter_next(&iter));

  /* Didn't find the field for messagetype */
  return false;
}

/*JSON{
  "type" : "library",
  "class" : "meshtastic",
}
*/

/*JSON{
  "type" : "staticmethod"
  "class" : "meshtastic",
  "ifdef" : "MESHTASTIC",
  "name" : "connect",
  "generate" : "jswrap_meshtastic",
  "params" : [
    ["uart", "JsVar", "Device to use for serial connection"]
  ]
}

Testing Meshtastic
*/
void jswrap_meshtastic(JsVar *uart)
{
  IOEventFlags uartDevice;
  if (uartDevice)
  {
    // Connection? UART, BLE?
    uartDevice = jsiGetDeviceFromClass(uart); // ? Only see it used for SPI

    if (!DEVICE_IS_SERIAL(uartDevice))
    {
      jsExceptionHere(JSET_ERROR, "Expecting serial device, got %q", uart);
      return 0;
    }
  }

  // send want_config_id

  /* This is the buffer where we will store our message. */
  uint8_t buffer[128];
  size_t message_length;
  bool status;

  {
    meshtastic_ToRadio toRadio = meshtastic_ToRadio_init_zero;

    // TODO: Could also stream straight to the UART tbh
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    // Fill data
    // TODO: Fill random id here and match it to the response
    if (!encode_unionmessage(stream, uint32_t, 123))
    {
      // TODO: Exception
      return 0;
    }

    /* Now we are ready to encode the message! */
    status = pb_encode(&stream, meshtastic_ToRadio_fields, &message);
    message_length = stream.bytes_written;

    /* Then just check for any errors.. */
    if (!status)
    {
      printf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
      return 1;
    }
  }

  // Packets always have a four byte header (described below) prefixed before each packet.
  // This header provides framing characters and length.
  // Byte 0: START1 (0x94)
  // Byte 1: START2 (0xc3)
  // Byte 2: MSB of protobuf length
  // Byte 3: LSB of protobuf length

  // 0x94, 0xc3
  // message_length

  // Now send it over uart, unless we buffer straight to it...
  // uartDevice

  // should be less length than 512

  // device will send back a whole pile of
  // meshtastic_FromRadio
  // until buffer empties
  // Make sure config_complete_id matches our initial specified ID

  // Seems like all traffic out is ToRadio and all traffic back is FromRadio

  // encode_unionmessage(stream,)
}