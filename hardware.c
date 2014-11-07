#include "hardware.h"
#include <stdlib.h>
#include <string.h>

#define NEW(x) ((x*) malloc(sizeof(x)))

struct hw_ent *hw_new() {
	struct hw_ent *out = NEW(struct hw_ent);
	if (out == NULL) {
		return NULL;
	}
	out->sysbus_head = NULL;
	return out;
}

static void hw_add_sysbus(struct hw_ent *out, struct hw_sysbus *add) {
	add->next = out->sysbus_head;
	out->sysbus_head = add;
}

static struct hw_sysbus *hw_sysbus_new(uint16_t typeid) {
	struct hw_sysbus *out = NEW(struct hw_sysbus);
	if (out == NULL) {
		return NULL;
	}
	out->busid = 0xFF;
	out->typeid = typeid;
	out->entry_data = NULL;
	out->next = NULL;
	return out;
}

struct hw_serial_buffer *hw_serial_new() {
	struct hw_serial_buffer *out = NEW(struct hw_serial_buffer);
	if (out == NULL) {
		return NULL;
	}
	memset(out->data, '\0', sizeof(out->data));
	out->read_ptr = out->write_ptr = 0;
	MUTEX_INIT(out->lock);
	return out;
}

void hw_serial_delete(struct hw_serial_buffer *buffer) {
	MUTEX_DESTROY(buffer->lock);
	free(buffer);
}

bool hw_serial_insert(struct hw_serial_buffer *ptr, uint8_t data) {
	bool could_write;
	MUTEX_LOCK(ptr->lock);
	could_write = (ptr->read_ptr != (uint8_t) (ptr->write_ptr + 1));
	if (could_write) {
		ptr->data[ptr->write_ptr++] = data;
	}
	MUTEX_UNLOCK(ptr->lock);
	return could_write;
}

bool hw_serial_remove(struct hw_serial_buffer *ptr, uint8_t *data) {
	bool could_read;
	MUTEX_LOCK(ptr->lock);
	could_read = (ptr->read_ptr != ptr->write_ptr);
	if (could_read) {
		*data = ptr->data[ptr->read_ptr++];
	}
	MUTEX_UNLOCK(ptr->lock);
	return could_read;
}

void hw_add_sysbus_serial_in(struct hw_ent *out, struct hw_serial_buffer *buffer) {
	struct hw_sysbus *n = hw_sysbus_new(HW_SYSBUS_SERIAL_IN);
	n->entry_data = buffer;
	hw_add_sysbus(out, n);
}

void hw_add_sysbus_serial_out(struct hw_ent *out, struct hw_serial_buffer *buffer) {
	struct hw_sysbus *n = hw_sysbus_new(HW_SYSBUS_SERIAL_OUT);
	n->entry_data = buffer;
	hw_add_sysbus(out, n);
}

void hw_add_sysbus_null(struct hw_ent *out) {
	hw_add_sysbus(out, hw_sysbus_new(HW_SYSBUS_NULL));
}

void hw_add_sysbus_zero(struct hw_ent *out) {
	hw_add_sysbus(out, hw_sysbus_new(HW_SYSBUS_ZERO));
}

struct hw_sysbus *hw_bus_lookup(struct hw_ent *out, uint8_t busid) {
	struct hw_sysbus *active = out->sysbus_head;
	while (active != NULL && active->busid != busid) {
		active = active->next;
	}
	return active;
}

uint16_t hw_exchange_bus(struct hw_sysbus *ent, uint8_t byte) {
	uint8_t toread;
	switch (ent->typeid) {
	case HW_SYSBUS_SERIAL_IN:
		if (hw_serial_remove((struct hw_serial_buffer *) ent->entry_data, &toread)) {
			return toread;
		} else {
			return 0xFFFF;
		}
	case HW_SYSBUS_SERIAL_OUT:
		return hw_serial_insert((struct hw_serial_buffer *) ent->entry_data, byte) ? 0x0001 : 0x0000;
	case HW_SYSBUS_ZERO:
		return 0x0000;
	default:
		return 0xFFFF;
	}
}

