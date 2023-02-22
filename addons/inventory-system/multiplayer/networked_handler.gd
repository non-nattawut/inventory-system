extends InventoryHandler
class_name NetworkedHandler

func _ready():
	if multiplayer.is_server():
		updated_transaction_slot.connect(_on_updated_transaction_slot.bind())


func _on_updated_transaction_slot(item_id : int, amount : int):
	_on_updated_transaction_slot_rpc.rpc(item_id, amount)


@rpc
func _on_updated_transaction_slot_rpc(item_id : int, amount : int):
	_set_transaction_slot(item_id, amount)


func drop(item : InventoryItem, amount := 1) -> bool:
	var item_id = database.get_id_from_item(item)
	if item_id <= 0:
		return false
	if not multiplayer.is_server():
		drop_rpc.rpc_id(1, item_id, amount)
	else:
		drop_rpc(item_id, amount)
	return true


# func add_to_inventory(inventory : Inventory, item : InventoryItem, amount := 1, drop_excess := false) -> int:
# 	var item_id = database.get_id_from_item(item)
# 	if item_id <= 0:
# 		return amount
# 	if not multiplayer.is_server():
# 		add_to_inventory_rpc.rpc_id(1, inventory.get_path(), item_id, amount, drop_excess)
# 	else:
# 		return super.add_to_inventory(inventory.get_path(), item_id, amount, drop_excess)
# 	return true


func pick_to_inventory(dropped_item, inventory := self.inventory):
	if not multiplayer.is_server():
		pick_to_inventory_rpc.rpc_id(1, dropped_item.get_path(), inventory.get_path())
	else:
		pick_to_inventory_rpc(dropped_item.get_path(), inventory.get_path())


func drop_transaction():
	if not multiplayer.is_server():
		drop_transaction_rpc.rpc_id(1)
	else:
		drop_transaction_rpc()


func open(inventory : Inventory) -> bool:
	if not multiplayer.is_server():
		open_rpc.rpc_id(1, inventory.get_path())
		emit_signal("opened", inventory)
	else:
		open_rpc(inventory.get_path())
	return true


func close(inventory : Inventory) -> bool:
	if not multiplayer.is_server():
		close_rpc.rpc_id(1, inventory.get_path())
	else:
		close_rpc(inventory.get_path())
	return true


func close_all_inventories():
	if not multiplayer.is_server():
		close_all_inventories_rpc.rpc_id(1)
	else:
		close_all_inventories_rpc()
	return true


func to_transaction(slot_index : int , inventory : Inventory, amount : int):
	if not multiplayer.is_server():
		to_transaction_rpc.rpc_id(1, slot_index, inventory.get_path(), amount)
	else:
		to_transaction_rpc(slot_index, inventory.get_path(), amount)


func transaction_to_at(slot_index : int, inventory : Inventory):
	if not multiplayer.is_server():
		transaction_to_at_rpc.rpc_id(1, slot_index, inventory.get_path())
	else:
		transaction_to_at_rpc(slot_index, inventory.get_path())


func transaction_to(inventory : Inventory):
	if not multiplayer.is_server():
		transaction_to_rpc.rpc_id(1, inventory.get_path())
	else:
		transaction_to_rpc(inventory.get_path())


# Commands to server from client

@rpc("any_peer")
func drop_rpc(item_id : int, amount : int):
	if not multiplayer.is_server():
		return
	var item = database.get_item(item_id)
	if item == null:
		return
	super.drop(item, amount)


@rpc("any_peer")
func add_to_inventory_rpc(object_path : NodePath, item_id : int, amount := 1, drop_excess := false):
	if not multiplayer.is_server():
		return
	var item = database.get_item(item_id)
	if item == null:
		return
	var object = get_node(object_path)
	if object == null:
		return
	var inventory = object as Inventory
	if inventory == null:
		return
	super.add_to_inventory(inventory, item, amount, drop_excess)


@rpc("any_peer")
func pick_to_inventory_rpc(pick_object_path : NodePath, object_path : NodePath):
	if not multiplayer.is_server():
		return
	var pick_object = get_node(pick_object_path)
	if pick_object == null:
		return
	var dropped_item = pick_object as DroppedItem
	if dropped_item == null:
		return
	var object = get_node(object_path)
	if object == null:
		return
	var inventory = object as Inventory
	if inventory == null:
		return
	super.pick_to_inventory(dropped_item, inventory)


@rpc("any_peer")
func drop_transaction_rpc():
	if not multiplayer.is_server():
		return
	super.drop_transaction()


@rpc("any_peer")
func open_rpc(object_path : NodePath):
	if not multiplayer.is_server():
		return
	var object = get_node(object_path)
	if object == null:
		return
	var inventory = object as Inventory
	if inventory == null:
		return
	var id = multiplayer.get_remote_sender_id()
	if super.open(inventory) and id > 1:
		_on_open_rpc.rpc_id(multiplayer.get_remote_sender_id(), object_path)


@rpc("any_peer")
func to_transaction_rpc(slot_index : int, object_path : NodePath, amount : int):
	if not multiplayer.is_server():
		return
	var object = get_node(object_path)
	if object == null:
		return
	var inventory = object as Inventory
	if inventory == null:
		return
	super.to_transaction(slot_index, inventory, amount)


@rpc("any_peer")
func transaction_to_at_rpc(slot_index : int, object_path : NodePath):
	if not multiplayer.is_server():
		return
	var object = get_node(object_path)
	if object == null:
		return
	var inventory = object as Inventory
	if inventory == null:
		return
	super.transaction_to_at(slot_index, inventory)


@rpc("any_peer")
func transaction_to_rpc(object_path : NodePath):
	if not multiplayer.is_server():
		return
	var object = get_node(object_path)
	if object == null:
		return
	var inventory = object as Inventory
	if inventory == null:
		return
	super.transaction_to(inventory)


@rpc("any_peer")
func close_all_inventories_rpc():
	super.close_all_inventories()


@rpc("any_peer")
func close_rpc(object_path : NodePath):
	if not multiplayer.is_server():
		return
	var object = get_node(object_path)
	if object == null:
		return
	var inventory = object as Inventory
	if inventory == null:
		return
	super.close(inventory)


# Responses from server to client

@rpc
func _on_open_rpc(object_path : NodePath):
	if not multiplayer.is_server():
		return
	var object = get_node(object_path)
	if object == null:
		return
	var inventory = object as Inventory
	if inventory == null:
		return
	emit_signal("opened", inventory)


## Returns [code]true[/code] if main [Inventory] is open.
func is_open_main_inventory() -> bool:
	return super.is_open(inventory)


## Open main [Inventory]. Return [code]true[/code] if opened successfully.
func open_main_inventory() -> bool:
	return super.open(inventory)


## Open main [Inventory]. Return [code]true[/code] if opened successfully.
func close_main_inventory() -> bool:
	return super.close(inventory)
