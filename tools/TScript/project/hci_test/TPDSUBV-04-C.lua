require "hci_reset"

handle = tl_usb_init(0xffff);
handle_bulk = tl_usb_bulk_monitor_init(0xffff);
--- print("handle : ",string.format("0x%04x",handle_bulk)) 
tl_usb_bulk_monitor_start(handle_bulk)

hci_reset(BLE_SUCCESS)

local answer
io.write("continue with this operation (y/n)? ")
repeat
   answer=tl_input_get()
until answer=='y'


print("end!")
tl_usb_bulk_monitor_end()


