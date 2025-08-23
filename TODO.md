# TODO List

## Completed Tasks ✅

- [x] **fix_event_struct** - Add size and transfer_ownership fields to Event struct
- [x] **update_publish_method** - Implement proper ownership transfer in TinyEventBus::publish()
- [x] **fix_dispatch_method** - Update dispatch() to handle ownership transfer
- [x] **update_event_creation** - Modify event creation to use transfer_ownership flag
- [x] **test_memory_fix** - Test the complete memory management fix
- [x] **adapt_eventbus_interface** - Update all code to use new human-readable EventBus interface

## Current Tasks 🔄

- [ ] **test_updated_eventbus** - Flash and test the updated EventBus with new interface

## Pending Tasks 📋

- [ ] **verify_memory_corruption_fix** - Verify that the memory corruption issue is resolved
- [ ] **monitor_queue_behavior** - Test queue full warnings and event dropping behavior
- [ ] **performance_testing** - Test performance with the new memory management approach
- [ ] **documentation_update** - Update documentation to reflect the new EventBus interface

## Notes

- The EventBus interface has been updated to use more human-readable method names:
  - `begin()` → `initialize()`
  - `subscribe()` → `addEventListener()`
  - `unsubscribe()` → `removeEventListener()`
  - `publish()` → `publishEvent()`
  - `publishFromISR()` → `publishEventFromInterrupt()`
- The `TinyMailbox` class has been renamed to `LatestValueMailbox` with updated method names
- All memory management issues have been addressed with proper ownership transfer
- The build is now successful and ready for testing
