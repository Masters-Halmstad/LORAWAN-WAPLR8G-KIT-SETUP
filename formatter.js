function decodeUplink(input) {
  if (input.bytes.length !== 2) {
    return {
      errors: ["Expected exactly 2 bytes for distance"]
    };
  }

  const distanceRaw = (input.bytes[0] << 8) | input.bytes[1];

  return {
    data: {
      distance_cm: distanceRaw / 10,
      distance_mm: distanceRaw
    }
  };
}