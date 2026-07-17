#ifndef EXTENSIONS_SLOT_REF_TRACKER_H
#define EXTENSIONS_SLOT_REF_TRACKER_H

#include <algorithm>
#include <vector>

namespace Extensions {

// CRTP mixin: tracks pointers-to-pointers (slot addresses) that reference
// this object. The destructor walks the list and NULLs each slot so external
// callers won't dereference freed memory. Inherit as a second base:
//   class LegoROI : public ROI, public SlotRefTracker<LegoROI> { ... };
template <typename T>
class SlotRefTracker {
public:
	// Exposes the tracker's parameter type so BindSlot/ClearSlot can auto-deduce
	// the right cast target. Subclasses inherit this typedef.
	using SlotType = T;

	void RegisterSlotRef(T** p_slot) { m_slotRefs.push_back(p_slot); }

	void UnregisterSlotRef(T** p_slot)
	{
		typename std::vector<T**>::iterator it = std::find(m_slotRefs.begin(), m_slotRefs.end(), p_slot);
		if (it != m_slotRefs.end()) {
			m_slotRefs.erase(it);
		}
	}

protected:
	// Protected non-virtual: only destructible via a derived class.
	// Fires automatically during the derived destructor chain.
	~SlotRefTracker()
	{
		for (T** slot : m_slotRefs) {
			*slot = NULL;
		}
	}

private:
	std::vector<T**> m_slotRefs;
};

// Bind `slot` to point at `p_object` and register the slot's address with the
// object's SlotRefTracker base. On the object's destruction, the slot will be
// auto-NULL'd. If `slot` already points at a registered object, it is first
// unregistered to avoid leaving a dangling back-ref to a slot that's about to
// reference a different object.
//
// TSlot may be the tracker's T or a subclass (single inheritance assumed —
// the static_cast<TSlot*>(p_object) and the reinterpret_cast on &slot rely on
// TSlot and TSlot::SlotType having identical pointer representations).
template <typename TSlot, typename TObject>
void BindSlot(TSlot*& slot, TObject* p_object)
{
	using TBase = typename TSlot::SlotType;
	if (slot) {
		slot->UnregisterSlotRef(reinterpret_cast<TBase**>(&slot));
	}
	slot = static_cast<TSlot*>(p_object);
	if (slot) {
		slot->RegisterSlotRef(reinterpret_cast<TBase**>(&slot));
	}
}

// Unregister `slot` from its referent's SlotRefTracker (if non-NULL) and set
// it to NULL. No-op if slot is already NULL.
template <typename TSlot>
void ClearSlot(TSlot*& slot)
{
	using TBase = typename TSlot::SlotType;
	if (slot) {
		slot->UnregisterSlotRef(reinterpret_cast<TBase**>(&slot));
		slot = NULL;
	}
}

} // namespace Extensions

#endif // EXTENSIONS_SLOT_REF_TRACKER_H
