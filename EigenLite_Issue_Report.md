### EigenLite Issue Report: Thread Safety and USB Cleanup Crashes

#### 1. The Issue: Thread Safety in `EigenLite::poll()` vs. `EigenLite::destroy()`

**Problem Description:**
The `EigenLite` class is not thread-safe when its `process()` (which calls `poll()`) and `destroy()` methods are called from different threads. In `ECMapper`, the `HardwareService` runs `process()` in a dedicated background thread, while `destroy()` (or `stop()`) is often triggered by the UI (Message Thread) during role switching or application shutdown.

Specifically, `EigenLite::poll()` iterates over the `devices_` vector:
```cpp
for (auto pDevice : devices_) { ret &= pDevice->poll(0); }
```
At the same time, `EigenLite::destroy()` modifies this vector:
```cpp
for (auto device : devices_) { device->destroy(); }
devices_.clear();
```
This leads to a race condition where one thread is accessing an `EF_Harp` object that another thread is destroying, or it is iterating over a vector that is being cleared, causing a segmentation fault.

**Proposed Fix:**
Introduce a `std::mutex` in `EigenLite` to protect the `devices_`, `availablePicos_`, `availableBaseStations_`, and `deadDevices_` collections. All methods that access or modify these members (like `poll()`, `destroy()`, `connectNewBaseStation()`, `setLED()`, etc.) should use a `std::lock_guard<std::mutex>`.

---

#### 2. The Issue: Null Pointer Dereference in `pic_usb_macosx.cpp`

**Problem Description:**
The `close_interface` and `close_device` functions in `picross/src/pic_usb_macosx.cpp` lacked sufficient safety checks for null pointers. During cleanup, if a device was partially initialized or already partially cleaned up, these functions could be called with pointers to null interfaces, leading to a crash when dereferencing them to call `USBInterfaceClose` or `Release`.

**Code at fault:**
```cpp
void close_interface(IOUSBDeviceInterface197 **device, IOUSBInterfaceInterface197 **interface)
{
    if(interface) // Only checks if the pointer-to-pointer is not null
    {
        (*interface)->USBInterfaceClose(interface); // Crashes if *interface is null
        (*interface)->Release(interface);
    }
    // ...
}
```

**Proposed Fix:**
Add checks to ensure the dereferenced pointers are also not null before attempting to call methods on them:
```cpp
void close_interface(IOUSBDeviceInterface197 **device, IOUSBInterfaceInterface197 **interface)
{
    if(interface && *interface)
    {
        (*interface)->USBInterfaceClose(interface);
        (*interface)->Release(interface);
    }
    if(device && *device)
    {
        (*device)->USBDeviceClose(device);
    }
}
```
Similarly for `close_device`:
```cpp
void close_device(IOUSBDeviceInterface197 **device)
{
    if(device && *device)
    {
        (*device)->Release(device);
    }
}
```

---

#### 3. The Issue: Stale Device Discovery State in `EigenLite::destroy()`

**Problem Description:**
When `EigenLite::destroy()` (called via `stop()`) is executed, it clears the `devices_` list but it does NOT clear the `availableBaseStations_` and `availablePicos_` vectors. These vectors store the list of USB devices found during the last discovery cycle.

When `EigenLite::create()` (called via `start()`) is subsequently called again (e.g., when switching roles in ECMapper), it starts a new discovery thread. The `checkUsbDev()` method in that thread compares the current USB devices with `availableBaseStations_`. If they match (which is likely if devices weren't unplugged), it does NOT set `usbDevChange_` to `true`.

Since `usbDevChange_` is false, the next call to `poll()` will NOT attempt to connect to the "new" devices. Because `devices_` was cleared in `destroy()`, the application will fail to re-detect and re-connect to the instruments until they are physically unplugged and re-plugged.

**Proposed Fix:**
In `EigenLite::destroy()`, ensure all cached discovery state is cleared:
```cpp
bool EigenLite::destroy() {
    // ... join thread ...
    for (auto device : devices_) { device->destroy(); }
    devices_.clear();
    
    // NEW: Clear discovery cache to force re-detection on next start
    availableBaseStations_.clear();
    availablePicos_.clear();
    usbDevChange_ = false;
    
    return true;
}
```

---

#### 4. Summary of Recommended Changes for `EigenLite`

1.  **In `eigenlite_impl.h`**:
    - Include `<mutex>`.
    - Add `std::mutex devicesMutex_;` as a private member of `EigenLite`.
2.  **In `eigenlite.cpp`**:
    - Wrap accesses to `devices_` and other shared state in `std::lock_guard<std::mutex> lock(devicesMutex_);`.
    - Clear `availableBaseStations_`, `availablePicos_`, and `usbDevChange_` in `destroy()`.
3.  **In `pic_usb_macosx.cpp`**:
    - Update `close_interface` and `close_device` with safety checks as described above.
