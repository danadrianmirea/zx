
/*  ZX Spectrum Emulation Module for Python.

    Copyright (C) 2017 Ivan Kosarev.
    ivan@kosarev.info

    Published under the MIT license.
*/

#include <Python.h>

#include <new>

#include "../zx.h"

namespace {

using zx::fast_u16;

namespace Spectrum48 {

struct machine_state {
    fast_u16 bc;
};

class machine_emulator : public zx::spectrum48 {
public:
    typedef zx::spectrum48 base;

    machine_emulator() {
        retrieve_state();
    }

    machine_state &get_machine_state() {
        return state;
    }

    void retrieve_state() {
        state.bc = get_bc();
    }

    void install_state() {
        set_bc(state.bc);
    }

    pixels_buffer_type &get_frame_pixels() {
        base::get_frame_pixels(pixels);
        return pixels;
    }

    void execute_frame() {
        install_state();
        base::execute_frame();
        retrieve_state();
    }

private:
    machine_state state;
    pixels_buffer_type pixels;
};

struct object_instance {
    PyObject_HEAD
    machine_emulator emulator;
};

static inline object_instance *cast_object(PyObject *p) {
    return reinterpret_cast<object_instance*>(p);
}

static inline machine_emulator &cast_emulator(PyObject *p) {
    return cast_object(p)->emulator;
}

PyObject *get_state(PyObject *self, PyObject *args) {
    auto &state = cast_emulator(self).get_machine_state();
    return PyMemoryView_FromMemory(reinterpret_cast<char*>(&state),
                                   sizeof(state), PyBUF_WRITE);
}

PyObject *get_memory(PyObject *self, PyObject *args) {
    auto &memory = cast_emulator(self).get_memory();
    return PyMemoryView_FromMemory(reinterpret_cast<char*>(memory),
                                   sizeof(memory), PyBUF_WRITE);
}

PyObject *render_frame(PyObject *self, PyObject *args) {
    auto &emulator = cast_emulator(self);
    emulator.render_frame();

    const auto &frame_chunks = emulator.get_frame_chunks();
    return PyMemoryView_FromMemory(
        const_cast<char*>(reinterpret_cast<const char*>(&frame_chunks)),
        sizeof(frame_chunks), PyBUF_READ);
}

static PyObject *get_frame_pixels(PyObject *self, PyObject *args) {
    auto &pixels = cast_emulator(self).get_frame_pixels();
    return PyMemoryView_FromMemory(reinterpret_cast<char*>(pixels),
                                   sizeof(pixels), PyBUF_READ);
}

PyObject *execute_frame(PyObject *self, PyObject *args) {
    cast_emulator(self).execute_frame();
    Py_RETURN_NONE;
}

PyMethodDef methods[] = {
    {"get_state", get_state, METH_NOARGS,
     "Return a MemoryView object that exposes the internal state of the "
     "simulated machine."},
    {"get_memory", get_memory, METH_NOARGS,
     "Return a MemoryView object that exposes the memory of the simulated "
     "machine."},
    {"render_frame", render_frame, METH_NOARGS,
     "Render current frame and return a MemoryView object that exposes a "
     "buffer that contains rendered data."},
    {"get_frame_pixels", get_frame_pixels, METH_NOARGS,
     "Convert rendered frame into an internally allocated array of RGB24 pixels "
     "and return a MemoryView object that exposes that array."},
    {"execute_frame", execute_frame, METH_NOARGS,
     "Execute instructions that correspond to a single frame."},
    { nullptr }  // Sentinel.
};

PyObject *object_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    if(!PyArg_ParseTuple(args, ":Spectrum48Base.__new__"))
        return nullptr;

    auto *self = cast_object(type->tp_alloc(type, /* nitems= */ 0));
    if(!self)
      return nullptr;

    auto &emulator = self->emulator;
    ::new(&emulator) zx::spectrum48();
    return &self->ob_base;
}

void object_dealloc(PyObject *self) {
    auto &object = *cast_object(self);
    object.emulator.~spectrum48();
    Py_TYPE(self)->tp_free(self);
}

static PyTypeObject type_object = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "zx._emulator.Spectrum48Base",
                                // tp_name
    sizeof(object_instance),    // tp_basicsize
    0,                          // tp_itemsize
    object_dealloc,             // tp_dealloc
    0,                          // tp_print
    0,                          // tp_getattr
    0,                          // tp_setattr
    0,                          // tp_reserved
    0,                          // tp_repr
    0,                          // tp_as_number
    0,                          // tp_as_sequence
    0,                          // tp_as_mapping
    0,                          // tp_hash
    0,                          // tp_call
    0,                          // tp_str
    0,                          // tp_getattro
    0,                          // tp_setattro
    0,                          // tp_as_buffer
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
                                // tp_flags
    "ZX Spectrum 48K Emulator", // tp_doc
    0,                          // tp_traverse
    0,                          // tp_clear
    0,                          // tp_richcompare
    0,                          // tp_weaklistoffset
    0,                          // tp_iter
    0,                          // tp_iternext
    methods,                    // tp_methods
    nullptr,                    // tp_members
    0,                          // tp_getset
    0,                          // tp_base
    0,                          // tp_dict
    0,                          // tp_descr_get
    0,                          // tp_descr_set
    0,                          // tp_dictoffset
    0,                          // tp_init
    0,                          // tp_alloc
    object_new,                 // tp_new
    0,                          // tp_free
    0,                          // tp_is_gc
    0,                          // tp_bases
    0,                          // tp_mro
    0,                          // tp_cache
    0,                          // tp_subclasses
    0,                          // tp_weaklist
    0,                          // tp_del
    0,                          // tp_version_tag
    0,                          // tp_finalize
};

}  // namespace Spectrum48

static PyModuleDef module = {
    PyModuleDef_HEAD_INIT,      // m_base
    "zx._emulator",             // m_name
    "ZX Spectrum Emulation Module",
                                // m_doc
    -1,                         // m_size
    nullptr,                    // m_methods
    nullptr,                    // m_slots
    nullptr,                    // m_traverse
    nullptr,                    // m_clear
    nullptr,                    // m_free
};

}  // anonymous namespace

extern "C" PyMODINIT_FUNC PyInit__emulator(void) {
    PyObject *m = PyModule_Create(&module);
    if(!m)
        return nullptr;

    if(PyType_Ready(&Spectrum48::type_object) < 0)
        return nullptr;
    Py_INCREF(&Spectrum48::type_object);

    // TODO: Check the returning value.
    PyModule_AddObject(m, "Spectrum48Base",
                       &Spectrum48::type_object.ob_base.ob_base);
    return m;
}
