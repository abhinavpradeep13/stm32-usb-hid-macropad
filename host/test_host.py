"""
Test the PC side without the board.

  python test_host.py          -> dry run: prints what each button WOULD do
  python test_host.py --real   -> actually launches the mapped apps/URLs
"""
import sys
import macropad_host as h

REAL = "--real" in sys.argv

if not REAL:
    h.run_action = lambda a: print(f"   [dry run] would run: {a['type']} -> {a['target']}")

# 1. stats packet (what the PC sends to the board)
pkt = h.build_packet(7)
print(f"stats packet: len={len(pkt)}  id={pkt[0]:#04x} cmd={pkt[1]}  "
      f"cpu={pkt[2]} ram={pkt[3]} bat={pkt[4]} temp={pkt[5]} seq={pkt[6]}")
assert len(pkt) == 64 and pkt[0] == 0x03

# 2. fake button reports (what the board sends to the PC): 64 bytes, ID 2
actions = h.load_actions()
def fake(app_id, event=1):
    return [0x02, app_id, event] + [0] * 61

print("\nbutton 0 press:");   h.handle_report(fake(0), actions)
print("button 1 press:");     h.handle_report(fake(1), actions)
print("button 2 press:");     h.handle_report(fake(2), actions)
print("button 2 release (ignored):"); h.handle_report(fake(2, 0), actions)
print("button 9 (unmapped):"); h.handle_report(fake(9), actions)
print("wrong report ID (ignored):"); h.handle_report([0x03, 0, 1] + [0] * 61, actions)

# 3. is the device visible? (only works with the board plugged in)
path = h.find_device_path()
print("\nboard found" if path else "\nboard not found (expected until the Black Pill arrives)")
