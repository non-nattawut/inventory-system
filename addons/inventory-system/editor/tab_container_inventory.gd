@tool
extends TabContainer

@export var items_icon : Texture2D
@export var recipes_icon : Texture2D

func _ready():
	set_tab_button_icon(0, items_icon)
	set_tab_button_icon(1, recipes_icon)
	get_tab_control(0).mouse_filter = Control.MOUSE_FILTER_PASS
	get_tab_control(1).mouse_filter = Control.MOUSE_FILTER_PASS
