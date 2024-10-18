#include "inventory.h"
#include <godot_cpp/classes/engine.hpp>

Inventory::Inventory() {
}

Inventory::~Inventory() {
}

void Inventory::_enter_tree() {
	// if (!Engine::get_singleton()->is_editor_hint()) {
	// 	_load_slots();
	// }
}

void Inventory::set_stack_content(const int stack_index, const String &item_id, const int &amount, const Dictionary &properties) {
	ERR_FAIL_COND_MSG(stack_index < 0 || stack_index >= size(), "The 'stack_index' is out of bounds.");
	ERR_FAIL_COND_MSG(amount < 0, "The 'amount' is negative.");

	int old_amount = this->amount();
	Ref<ItemStack> stack = items[stack_index];
	stack->set_item_id(item_id);
	stack->set_amount(amount);
	stack->set_properties(properties);
	items[stack_index] = stack;
	emit_signal("updated_stack", stack_index);
	_call_events(old_amount);
}

bool Inventory::is_empty() const {
	return amount() == 0;
}

bool Inventory::is_full() const {
	for (size_t i = 0; i < items.size(); i++) {
		Ref<ItemStack> stack = items[i];
		Ref<ItemDefinition> definition = get_database()->get_item(stack->get_item_id());
		if (definition != nullptr && stack->get_amount() < definition->get_max_stack())
			return false;
	}
	return true;
}

int Inventory::size() const {
	return items.size();
}

bool Inventory::contains(const String &item_id, const int &amount) const {
	ERR_FAIL_COND_V_MSG(amount < 0, false, "'amount' is negative.");

	int amount_in_inventory = 0;
	for (size_t i = 0; i < items.size(); i++) {
		Ref<ItemStack> stack = items[i];
		if (stack->contains(item_id, 1)) {
			amount_in_inventory += stack->get_amount();
			if (amount_in_inventory >= amount) {
				return true;
			}
		}
	}
	return false;
}

bool Inventory::contains_at(const int &stack_index, const String &item_id, const int &amount) const {
	ERR_FAIL_COND_V_MSG(stack_index < 0 || stack_index >= size(), false, "The 'slot index' is out of bounds.");
	ERR_FAIL_COND_V_MSG(amount < 0, false, "The 'amount' is negative.");

	if (stack_index < items.size()) {
		Ref<ItemStack> stack = items[stack_index];
		if (stack->contains(item_id, 1)) {
			return stack->get_amount() >= amount;
		}
	}
	return false;
}

bool Inventory::contains_category(const Ref<ItemCategory> &category, const int &amount) const {
	ERR_FAIL_NULL_V_MSG(category, false, "'category' is null.");
	ERR_FAIL_COND_V_MSG(amount < 0, false, "The 'amount' is negative.");

	int amount_in_inventory = 0;
	for (size_t i = 0; i < items.size(); i++) {
		Ref<ItemStack> stack = items[i];
		if (contains_category_in_stack(stack, category)) {
			amount_in_inventory += stack->get_amount();
			if (amount_in_inventory >= amount) {
				return true;
			}
		}
	}
	return false;
}

int Inventory::get_stack_index_with_an_item_of_category(const Ref<ItemCategory> &category) const {
	ERR_FAIL_NULL_V_MSG(category, 0, "'category' is null.");

	int amount_in_inventory = 0;
	for (size_t i = 0; i < items.size(); i++) {
		Ref<ItemStack> stack = items[i];
		if (contains_category_in_stack(stack, category)) {
			return i;
		}
	}
	return -1;
}

int Inventory::amount_of_item(const String &item_id) const {
	int amount_in_inventory = 0;
	for (size_t i = 0; i < items.size(); i++) {
		Ref<ItemStack> stack = items[i];
		if (stack->contains(item_id, 1)) {
			amount_in_inventory += stack->get_amount();
		}
	}
	return amount_in_inventory;
}

int Inventory::amount_of_category(const Ref<ItemCategory> &category) const {
	ERR_FAIL_NULL_V_MSG(category, 0, "'category' is null.");

	int amount_in_inventory = 0;
	for (size_t i = 0; i < items.size(); i++) {
		Ref<ItemStack> stack = items[i];
		if (stack == nullptr) {
			continue;
		}
		if (contains_category_in_stack(stack, category)) {
			amount_in_inventory += stack->get_amount();
		}
	}
	return amount_in_inventory;
}

int Inventory::amount() const {
	int amount_in_inventory = 0;
	for (size_t i = 0; i < items.size(); i++) {
		Ref<ItemStack> stack = items[i];
		if (stack == nullptr) {
			continue;
		}
		amount_in_inventory += stack->get_amount();
	}
	return amount_in_inventory;
}

int Inventory::add(const String &item_id, const int &amount, const Dictionary &properties, const bool &drop_excess) {
	ERR_FAIL_COND_V_MSG(amount < 0, amount, "The 'amount' is negative.");

	int amount_in_interact = amount;
	int old_amount = this->amount();

	for (size_t i = 0; i < items.size(); i++) {
		int previous_amount = amount_in_interact;
		amount_in_interact = _add_to_stack(i, item_id, amount_in_interact, properties);

		// Check for potential integer underflow
		ERR_FAIL_COND_V_MSG(amount_in_interact > previous_amount, amount, "Integer underflow detected in _add_to_slot.");

		if (amount_in_interact == 0) {
			break;
		}
	}

	if (amount_in_interact > 0) {
		_insert_stack(items.size());
		int previous_amount = amount_in_interact;
		amount_in_interact = _add_to_stack(items.size() - 1, item_id, amount_in_interact, properties);

		// Check for potential integer underflow
		ERR_FAIL_COND_V_MSG(amount_in_interact > previous_amount, amount, "Integer underflow detected in _add_to_slot after creating new slot.");

		_call_events(old_amount);
	}

	// Use subtraction to avoid potential overflow
	int _added = amount - amount_in_interact;

	// Sanity check
	ERR_FAIL_COND_V_MSG(_added < 0 || _added > amount, amount, "Invalid _added value calculated.");

	if (_added > 0) {
		emit_signal("item_added", item_id, _added);
	}

	if (drop_excess) {
		drop(item_id, amount_in_interact, properties);
		return 0;
	}

	return amount_in_interact;
}

int Inventory::add_at(const int &stack_index, const String &item_id, const int &amount, const Dictionary &properties) {
	ERR_FAIL_COND_V_MSG(stack_index < 0 || stack_index >= size(), amount, "The 'slot index' is out of bounds.");
	ERR_FAIL_COND_V_MSG(amount < 0, amount, "The 'amount' is negative.");

	int amount_in_interact = amount;
	int old_amount = this->amount();
	if (stack_index < items.size()) {
		amount_in_interact = _add_to_stack(stack_index, item_id, amount_in_interact, properties);
		_call_events(old_amount);
	}
	int _added = amount - amount_in_interact;
	if (_added > 0) {
		emit_signal("item_added", item_id, _added);
	}
	return amount_in_interact;
}

int Inventory::remove(const String &item_id, const int &amount) {
	ERR_FAIL_COND_V_MSG(amount < 0, amount, "The 'amount' is negative.");

	int amount_in_interact = amount;
	int old_amount = this->amount();
	for (size_t i = 0; i < items.size(); i++) {
		Ref<ItemStack> stack = items[i];
		amount_in_interact = _remove_from_stack(i, item_id, amount_in_interact);
		if (stack->get_amount() == 0) {
			_remove_stack_at(i);
			_call_events(old_amount);
		}
		if (amount_in_interact == 0) {
			break;
		}
	}
	int _removed = amount - amount_in_interact;
	if (_removed > 0) {
		emit_signal("item_removed", item_id, _removed);
	}
	return amount_in_interact;
}

int Inventory::remove_at(const int &stack_index, const String &item_id, const int &amount) {
	ERR_FAIL_COND_V_MSG(stack_index < 0 || stack_index >= size(), amount, "The 'slot index' is out of bounds.");
	ERR_FAIL_COND_V_MSG(amount < 0, amount, "The 'amount' is negative.");

	int amount_in_interact = amount;
	int old_amount = this->amount();
	if (stack_index < items.size()) {
		Ref<ItemStack> stack = items[stack_index];
		amount_in_interact = _remove_from_stack(stack_index, item_id, amount_in_interact);
		if (stack->get_amount() == 0) {
			_remove_stack_at(stack_index);
			_call_events(old_amount);
		}
	}
	int _removed = amount - amount_in_interact;
	if (_removed > 0) {
		emit_signal("item_removed", item_id, _removed);
	}
	return amount_in_interact;
}

void Inventory::transfer(const int &stack_index, Inventory *destination, const int &destination_stack_index, const int &amount) {
	ERR_FAIL_COND_MSG(stack_index < 0 || stack_index >= size(), "The 'stack index' is out of bounds.");
	ERR_FAIL_NULL_MSG(destination, "Destination inventory is null on transfer.");
	ERR_FAIL_NULL_MSG(get_database(), "InventoryDatabase is null.");
	ERR_FAIL_NULL_MSG(destination->get_database(), "InventoryDatabase is null.");
	ERR_FAIL_COND_MSG(get_database() != destination->get_database(), "Operation between inventories that do not have the same database is invalid.");
	ERR_FAIL_COND_MSG(destination_stack_index >= destination->size() || destination_stack_index < 0, "The 'destination stack index' exceeds the destination inventory size or negative value.");
	ERR_FAIL_COND_MSG(amount < 0, "The 'amount' is negative.");

	Ref<ItemStack> stack = items[stack_index];
	String item_id = stack->get_item_id();
	Dictionary properties = stack->get_properties();
	int amount_to_interact = amount;

	if (amount_to_interact == -1) {
		amount_to_interact = stack->get_amount();
	}
	Ref<ItemStack> destination_stack = destination->get_items()[destination_stack_index];
	Ref<ItemDefinition> destination_definition = get_database()->get_item(destination_stack->get_item_id());
	ERR_FAIL_NULL_MSG(destination_definition, "Destination item_definition is null on transfer.");
	int amount_to_left = destination_definition->get_max_stack() - destination_stack->get_amount();
	if (amount_to_left > -1) {
		amount_to_interact = MIN(amount_to_interact, amount_to_left);
	}
	if (amount_to_interact == 0)
		return;
	int amount_not_removed = remove_at(stack_index, item_id, amount_to_interact);
	int amount_to_transfer = amount_to_interact - amount_not_removed;
	if (amount_to_transfer == 0)
		return;
	int amount_not_transferred = destination->add_at(destination_stack_index, item_id, amount_to_transfer, properties);
	if (amount_not_transferred == 0)
		return;
	add_at(stack_index, item_id, amount_not_transferred, properties);
}

void Inventory::set_items(const TypedArray<ItemStack> &new_items) {
	items = new_items;
}

TypedArray<ItemStack> Inventory::get_items() const {
	return items;
}

void Inventory::set_slot_amount(const int &new_slot_amount) {
	slot_amount = new_slot_amount;
}

int Inventory::get_slot_amount() const {
	return slot_amount;
}

void Inventory::set_inventory_name(const String &new_inventory_name) {
	inventory_name = new_inventory_name;
}

String Inventory::get_inventory_name() const {
	return inventory_name;
}

Dictionary Inventory::serialize() const {
	Dictionary data = Dictionary();
	data["items"] = get_database()->serialize_item_stacks(items);
	return data;
}

void Inventory::deserialize(const Dictionary data) {
	ERR_FAIL_COND_MSG(!data.has("items"), "Data to deserialize is invalid: Does not contain the 'items' field");
	Array items_data = data["items"];
	get_database()->deserialize_item_stacks(items, items_data);
}

bool Inventory::drop(const String &item_id, const int &amount, const Dictionary &properties) {
	ERR_FAIL_NULL_V_MSG(get_database(), false, "'database' is null.");
	Ref<ItemDefinition> _definition = get_database()->get_item(item_id);
	ERR_FAIL_NULL_V_MSG(_definition, false, "'item_definition' is null.");
	if (_definition->get_properties().has("dropped_item")) {
		String path = _definition->get_properties()["dropped_item"];
		// We have i < 1000 to have some enforced upper limit preventing long loops
		for (size_t i = 0; i < amount && i < 1000; i++) {
			emit_signal("request_drop_obj", path, item_id, properties);
		}
		return true;
	}
	return false;
}

void Inventory::drop_from_inventory(const int &stack_index, const int &amount, const Dictionary &properties) {
	ERR_FAIL_COND(stack_index < 0 || stack_index >= items.size());

	if (items.size() <= stack_index)
		return;
	Ref<ItemStack> stack = items[stack_index];
	String item_id = stack->get_item_id();
	int not_removed = remove_at(stack_index, item_id, amount);
	int removed = amount - not_removed;
	drop(item_id, removed, properties);
}

int Inventory::add_to_stack(Ref<ItemStack> stack, const String &item_id, const int &amount, const Dictionary &properties) {
	ERR_FAIL_COND_V_MSG(amount < 0, 0, "The 'amount' is negative.");

	ERR_FAIL_NULL_V_MSG(get_database(), amount, "The 'database' is null.");
	Ref<ItemDefinition> definition = get_database()->get_item(item_id);
	ERR_FAIL_NULL_V_MSG(definition, amount, "The 'definition' is null.");

	// if (stack->is_categorized()) {
	// 	int flag_category = get_flag_categories_of_slot(slot);
	// 	if (flag_category != 0 && !is_accept_any_categories(flag_category, definition->get_categories())) {
	// 		return amount;
	// 	}
	// }
	if (amount <= 0) {
		return amount;
	}
	if (stack->has_valid() && (stack->get_item_id() != item_id || stack->get_properties() != properties)) {
		return amount;
	}

	int max_stack = definition->get_max_stack();
	int amount_to_add = MIN(amount, max_stack - stack->get_amount());
	stack->set_amount(stack->get_amount() + amount_to_add);
	stack->set_item_id(item_id);
	stack->set_properties(properties);
	stack->emit_signal("updated");
	return amount - amount_to_add;
}

int Inventory::remove_from_stack(Ref<ItemStack> stack, const String &item_id, const int &amount) {
	if (stack->get_item_id() == "") {
		return amount;
	}
	if (amount <= 0 || stack->get_item_id() != item_id) {
		return amount;
	}
	int amount_to_remove = MIN(amount, stack->get_amount());
	stack->set_amount(stack->get_amount() - amount_to_remove);
	stack->emit_signal("updated");
	return amount - amount_to_remove;
}

bool Inventory::is_accept_any_categories(const int categories_flag, const TypedArray<ItemCategory> &other_list) const {
	for (size_t i = 0; i < other_list.size(); i++) {
		Ref<ItemCategory> c = other_list[i];
		if (c == nullptr)
			continue;
		if ((categories_flag & c->get_code()) > 0) {
			return true;
		}
	}
	return false;
}

int Inventory::get_max_stack_of_stack(const Ref<ItemStack> &stack, Ref<ItemDefinition> &item) const {
	// if (slot->get_max_stack() == -1 && item != nullptr) {
	return item->get_max_stack();
	// } else {
	// 	return slot->get_max_stack();
	// }
}

bool Inventory::contains_category_in_stack(const Ref<ItemStack> &stack, const Ref<ItemCategory> &category) const {
	Ref<ItemDefinition> definition = get_database()->get_item(stack->get_item_id());
	if (definition == nullptr) {
		return false;
	} else {
		return definition->is_in_category(category);
	}
}

void Inventory::_insert_stack(int stack_index) {
	ERR_FAIL_COND_MSG(stack_index < 0 || stack_index > size(), "The 'stack index' is out of bounds.");

	Ref<ItemStack> stack = memnew(ItemStack());
	stack->set_item_id("");
	stack->set_amount(0);
	items.insert(stack_index, stack);
	this->emit_signal("stack_added", stack_index);
}

void Inventory::_remove_stack_at(int stack_index) {
	ERR_FAIL_COND_MSG(stack_index < 0 || stack_index >= size(), "The 'stack index' is out of bounds.");

	items.remove_at(stack_index);
	this->emit_signal("stack_removed", stack_index);
}

void Inventory::_call_events(int old_amount) {
	int actual_amount = amount();
	if (old_amount != actual_amount) {
		emit_signal("inventory_changed");
		if (is_empty()) {
			emit_signal("emptied");
		}
		if (is_full()) {
			emit_signal("filled");
		}
	}
}

int Inventory::_add_to_stack(int stack_index, const String &item_id, int amount, const Dictionary &properties) {
	ERR_FAIL_COND_V_MSG(amount < 0, amount, "The 'amount' is negative.");
	ERR_FAIL_COND_V_MSG(stack_index < 0 || stack_index >= size(), amount, "The 'slot index' is out of bounds.");

	Ref<ItemStack> stack = items[stack_index];
	ERR_FAIL_NULL_V_MSG(stack, amount, "The 'stack' is null.");

	int _remaining_amount = add_to_stack(stack, item_id, amount, properties);

	if (_remaining_amount == amount) {
		return amount;
	}

	emit_signal("updated_stack", stack_index);
	return _remaining_amount;
}

int Inventory::_remove_from_stack(int stack_index, const String &item_id, int amount) {
	ERR_FAIL_COND_V_MSG(stack_index < 0 || stack_index >= size(), amount, "The 'slot index' is out of bounds.");
	ERR_FAIL_COND_V_MSG(amount < 0, amount, "The 'amount' is negative.");

	Ref<ItemStack> stack = items[stack_index];
	int _remaining_amount = remove_from_stack(stack, item_id, amount);
	if (_remaining_amount == amount) {
		return amount;
	}
	emit_signal("updated_stack", stack_index);
	return _remaining_amount;
}

void Inventory::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stack_content", "stack_index", "item_id", "amount", "properties"), &Inventory::set_stack_content, DEFVAL(1), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("is_empty"), &Inventory::is_empty);
	ClassDB::bind_method(D_METHOD("is_full"), &Inventory::is_full);
	ClassDB::bind_method(D_METHOD("size"), &Inventory::size);
	ClassDB::bind_method(D_METHOD("contains", "item_id", "amount"), &Inventory::contains, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("contains_at", "stack_index", "item_id", "amount"), &Inventory::contains_at, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("contains_category", "category", "amount"), &Inventory::contains_category, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("get_stack_index_with_an_item_of_category", "category"), &Inventory::get_stack_index_with_an_item_of_category);
	ClassDB::bind_method(D_METHOD("amount_of_item", "item_id"), &Inventory::amount_of_item);
	ClassDB::bind_method(D_METHOD("get_amount_of_category", "category"), &Inventory::amount_of_category);
	ClassDB::bind_method(D_METHOD("get_amount"), &Inventory::amount);
	ClassDB::bind_method(D_METHOD("add", "item_id", "amount", "properties", "drop_excess"), &Inventory::add, DEFVAL(1), DEFVAL(Dictionary()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("add_at", "stack_index", "item_id", "amount", "properties"), &Inventory::add_at, DEFVAL(1), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("remove", "item_id", "amount"), &Inventory::remove, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("remove_at", "stack_index", "item_id", "amount"), &Inventory::remove_at, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("transfer", "stack_index", "destination", "destination_stack_index", "amount"), &Inventory::transfer, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("drop", "item_id", "amount", "properties"), &Inventory::drop, DEFVAL(1), DEFVAL(Dictionary()), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("drop_from_inventory", "stack_index", "amount", "properties"), &Inventory::drop_from_inventory, DEFVAL(1), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("add_to_stack", "stack", "item_id", "amount", "properties"), &Inventory::add_to_stack, DEFVAL(1), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("remove_from_stack", "stack", "item_id", "amount"), &Inventory::remove_from_stack, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("is_accept_any_categories", "categories_flag", "slot"), &Inventory::is_accept_any_categories);
	ClassDB::bind_method(D_METHOD("contains_category_in_stack", "stack", "category"), &Inventory::contains_category_in_stack);
	ClassDB::bind_method(D_METHOD("serialize"), &Inventory::serialize);
	ClassDB::bind_method(D_METHOD("deserialize", "data"), &Inventory::deserialize);

	ClassDB::bind_method(D_METHOD("set_items", "items"), &Inventory::set_items);
	ClassDB::bind_method(D_METHOD("get_items"), &Inventory::get_items);
	ClassDB::bind_method(D_METHOD("set_slot_amount", "slot_amount"), &Inventory::set_slot_amount);
	ClassDB::bind_method(D_METHOD("get_slot_amount"), &Inventory::get_slot_amount);
	ClassDB::bind_method(D_METHOD("set_inventory_name", "inventory_name"), &Inventory::set_inventory_name);
	ClassDB::bind_method(D_METHOD("get_inventory_name"), &Inventory::get_inventory_name);
	ClassDB::bind_method(D_METHOD("update_stack", "stack_index"), &Inventory::update_stack);
	ADD_SIGNAL(MethodInfo("inventory_changed"));
	ADD_SIGNAL(MethodInfo("stack_added", PropertyInfo(Variant::INT, "stack_index")));
	ADD_SIGNAL(MethodInfo("stack_removed", PropertyInfo(Variant::INT, "stack_index")));
	ADD_SIGNAL(MethodInfo("item_added", PropertyInfo(Variant::STRING, "item_id"), PropertyInfo(Variant::INT, "amount")));
	ADD_SIGNAL(MethodInfo("item_removed", PropertyInfo(Variant::STRING, "item_id"), PropertyInfo(Variant::INT, "amount")));
	ADD_SIGNAL(MethodInfo("filled"));
	ADD_SIGNAL(MethodInfo("emptied"));
	ADD_SIGNAL(MethodInfo("updated_stack", PropertyInfo(Variant::INT, "stack_index")));

	ADD_SIGNAL(MethodInfo("request_drop_obj", PropertyInfo(Variant::STRING, "drop_item_packed_scene_path"), PropertyInfo(Variant::STRING, "item_id"), PropertyInfo(Variant::DICTIONARY, "item_properties")));

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "items", PROPERTY_HINT_ARRAY_TYPE, vformat("%s/%s:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, "ItemStack")), "set_items", "get_items");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "slot_amount"), "set_slot_amount", "get_slot_amount");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "inventory_name"), "set_inventory_name", "get_inventory_name");
}

void Inventory::update_stack(const int stack_index) {
	emit_signal("updated_stack", stack_index);
	_call_events(amount());
}
