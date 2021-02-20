import Foundation

var shouldQuit: sig_atomic_t = 0

signal(SIGINT) { signal in shouldQuit = 1 }

let iso8601DateFormatter = ISO8601DateFormatter()
iso8601DateFormatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]

let r = co2mon_init(0)
let dev: co2mon_device = co2mon_open_device()

dev_loop_init(dev)

while true {
  
  if shouldQuit == 1 { break }
  
  let rdng = read_one(dev)
  if (rdng.message == OK) {
    if (rdng.sensor == temperature) {
      print(String(format: "{ \"ts\": \"\(iso8601DateFormatter.string(from: Date()))\", \"temp\": %3.1f }", rdng.value))
    } else if (rdng.sensor == CO2) {
      print(String(format: "{ \"ts\": \"\(iso8601DateFormatter.string(from: Date()))\", \"co2\": %d }", Int(rdng.value)))
    }
  }
  
  if shouldQuit == 1 { break }

}

co2mon_close_device(dev)
co2mon_exit()
