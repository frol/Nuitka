//
//     Copyright 2010, Kay Hayen, mailto:kayhayen@gmx.de
//
//     Part of "Nuitka", an attempt of building an optimizing Python compiler
//     that is compatible and integrates with CPython, but also works on its
//     own.
//
//     If you submit Kay Hayen patches to this software in either form, you
//     automatically grant him a copyright assignment to the code, or in the
//     alternative a BSD license to the code, should your jurisdiction prevent
//     this. Obviously it won't affect code that comes to him indirectly or
//     code you don't submit to him.
//
//     This is to reserve my ability to re-license the code at any time, e.g.
//     the PSF. With this version of Nuitka, using it for Closed Source will
//     not be allowed.
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, version 3 of the License.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//     Please leave the whole of this copyright notice intact.
//

#include "nuitka/prelude.hpp"

static PythonBuiltin _python_builtin_compile( "compile" );

PyObject *COMPILE_CODE( PyObject *source_code, PyObject *file_name, PyObject *mode, int flags )
{
    // May be a source, but also could already be a compiled object, in which case this
    // should just return it.
    if ( PyCode_Check( source_code ) )
    {
        return INCREASE_REFCOUNT( source_code );
    }

    // Workaround leading whitespace causing a trouble to compile builtin, but not eval builtin
    PyObject *source;

    if ( ( PyString_Check( source_code ) || PyUnicode_Check( source_code ) ) && strcmp( Nuitka_String_AsString( mode ), "exec" ) != 0 )
    {
        static PyObject *strip_str = PyString_FromString( "strip" );

        // TODO: There is an API to call a method, use it instead.
        source = LOOKUP_ATTRIBUTE( source_code, strip_str );
        source = PyObject_CallFunctionObjArgs( source, NULL );

        assert( source );
    }
    else if ( PyFile_Check( source_code ) && strcmp( Nuitka_String_AsString( mode ), "exec" ) == 0 )
    {
        static PyObject *read_str = PyString_FromString( "read" );

        // TODO: There is an API to call a method, use it instead.
        source = LOOKUP_ATTRIBUTE( source_code, read_str );
        source = PyObject_CallFunctionObjArgs( source, NULL );

        assert( source );
    }
    else
    {
        source = source_code;
    }

    PyObjectTemporary future_flags( PyInt_FromLong( flags ) );

    return _python_builtin_compile.call(
        _python_bool_True,       // dont_inherit
        future_flags.asObject(), // flags
        mode,
        file_name,
        source
    );
}

static PythonBuiltin _python_builtin_open( "open" );

PyObject *OPEN_FILE( PyObject *file_name, PyObject *mode, PyObject *buffering )
{
    if ( file_name == NULL )
    {
        return _python_builtin_open.call();

    }
    else if ( mode == NULL )
    {
        return _python_builtin_open.call(
           file_name
        );

    }
    else if ( buffering == NULL )
    {
        return _python_builtin_open.call(
           mode,
           file_name
        );
    }
    else
    {
        return _python_builtin_open.call(
           buffering,
           mode,
           file_name
        );
    }
}

PyObject *CHR( PyObject *value )
{
    long x = PyInt_AsLong( value );

    if ( x < 0 || x >= 256 )
    {
        PyErr_Format( PyExc_ValueError, "chr() arg not in range(256)" );
        throw _PythonException();
    }

    // TODO: A switch statement might be faster, because no object needs to be created at
    // all, this is how CPython does it.
    char s[1];
    s[0] = (char)x;

    return PyString_FromStringAndSize( s, 1 );
}

PyObject *ORD( PyObject *value )
{
    long result;

    if (likely( PyString_Check( value ) ))
    {
        Py_ssize_t size = PyString_GET_SIZE( value );

        if (likely( size == 1 ))
        {
            result = long( ((unsigned char *)PyString_AS_STRING( value ))[0] );
        }
        else
        {
            PyErr_Format( PyExc_TypeError, "ord() expected a character, but string of length %zd found", size );
            throw _PythonException();
        }
    }
    else if ( PyByteArray_Check( value ) )
    {
        Py_ssize_t size = PyByteArray_GET_SIZE( value );

        if (likely( size == 1 ))
        {
            result = long( ((unsigned char *)PyByteArray_AS_STRING( value ))[0] );
        }
        else
        {
            PyErr_Format( PyExc_TypeError, "ord() expected a character, but byte array of length %zd found", size );
            throw _PythonException();
        }
    }
    else if ( PyUnicode_Check( value ) )
    {
        Py_ssize_t size = PyUnicode_GET_SIZE( value );

        if (likely( size == 1 ))
        {
            result = long( *PyUnicode_AS_UNICODE( value ) );
        }
        else
        {
            PyErr_Format( PyExc_TypeError, "ord() expected a character, but unicode string of length %zd found", size );
            throw _PythonException();
        }
    }
    else
    {
        PyErr_Format( PyExc_TypeError, "ord() expected string of length 1, but %s found", value->ob_type->tp_name );
        throw _PythonException();
    }

    return PyInt_FromLong( result );
}

PyObject *BUILTIN_TYPE1( PyObject *arg )
{
    return INCREASE_REFCOUNT( (PyObject *)Py_TYPE( arg ) );
}

PyObject *BUILTIN_TYPE3( PyObject *module_name, PyObject *name, PyObject *bases, PyObject *dict )
{

    PyObject *result = PyType_Type.tp_new( &PyType_Type, PyObjectTemporary( MAKE_TUPLE( dict, bases, name ) ).asObject(), NULL );

    if (unlikely( result == NULL ))
    {
        throw _PythonException();
    }

    PyTypeObject *type = result->ob_type;

    if (likely( PyType_IsSubtype( type, &PyType_Type ) ))
    {
        if ( PyType_HasFeature( type, Py_TPFLAGS_HAVE_CLASS ) && type->tp_init != NULL )
        {
            int res = type->tp_init( result, MAKE_TUPLE( dict, bases, name ), NULL );

            if (unlikely( res < 0 ))
            {
                Py_DECREF( result );
                throw _PythonException();
            }
        }
    }

    int res = PyObject_SetAttr( result, _python_str_plain___module__, module_name );

    if ( res == -1 )
    {
        throw _PythonException();
    }

    return result;
}

PyObject *BUILTIN_RANGE( long low, long high, long step )
{
    assert( step != 0 );

    PyObject *result = MAKE_LIST();

    if ( step > 0 )
    {
        for( long value = low; value < high; value += step )
        {
            // TODO: Could and should create the list pre-sized and not append.
            PyObjectTemporary number( PyInt_FromLong( value ) );

            int res = PyList_Append( result, number.asObject() );
            assert( res == 0 );
        }
    }
    else
    {
        for( long value = low; value > high; value += step )
        {
            // TODO: Could and should create the list pre-sized and not append.
            PyObjectTemporary number( PyInt_FromLong( value ) );

            int res = PyList_Append( result, number.asObject() );
            assert( res == 0 );
        }
    }

    return result;
}

PyObject *BUILTIN_RANGE( long low, long high )
{
    return BUILTIN_RANGE( low, high, 1 );
}

PyObject *BUILTIN_RANGE( long boundary )
{
    return BUILTIN_RANGE( 0, boundary );
}

static PyObject *TO_RANGE_ARG( PyObject *value, char const *name )
{
    if (likely( PyInt_Check( value ) || PyLong_Check( value )) )
    {
        return INCREASE_REFCOUNT( value );
    }

    PyTypeObject *type = value->ob_type;
    PyNumberMethods *tp_as_number = type->tp_as_number;

    // Everything that casts to int is allowed.
    if ( tp_as_number == NULL || tp_as_number->nb_int == NULL )
    {
        PyErr_Format( PyExc_TypeError, "range() integer %s argument expected, got %s.", name, type->tp_name );
        throw _PythonException();
    }

    PyObject *result = tp_as_number->nb_int( value );

    if (unlikely( result == NULL ))
    {
        throw _PythonException();
    }

    return result;
}

static PythonBuiltin _python_builtin_range( "range" );

PyObject *BUILTIN_RANGE( PyObject *boundary )
{
    PyObjectTemporary boundary_temp( TO_RANGE_ARG( boundary, "start" ) );

    long start = PyInt_AsLong( boundary_temp.asObject() );

    if ( start == -1 && PyErr_Occurred() )
    {
        PyErr_Clear();

        return _python_builtin_range.call( boundary_temp.asObject() );
    }

    return BUILTIN_RANGE( start );
}

PyObject *BUILTIN_RANGE( PyObject *low, PyObject *high )
{
    PyObjectTemporary low_temp( TO_RANGE_ARG( low, "start" ) );
    PyObjectTemporary high_temp( TO_RANGE_ARG( high, "end" ) );

    bool fallback = false;

    long start = PyInt_AsLong( low_temp.asObject() );

    if (unlikely( start == -1 && PyErr_Occurred() ))
    {
        PyErr_Clear();
        fallback = true;
    }

    long end = PyInt_AsLong( high_temp.asObject() );

    if (unlikely( end == -1 && PyErr_Occurred() ))
    {
        PyErr_Clear();
        fallback = true;
    }

    if ( fallback )
    {
        return _python_builtin_range.call( high_temp.asObject(), low_temp.asObject() );
    }
    else
    {
        return BUILTIN_RANGE( start, end );
    }
}

PyObject *BUILTIN_RANGE( PyObject *low, PyObject *high, PyObject *step )
{
    PyObjectTemporary low_temp( TO_RANGE_ARG( low, "start" ) );
    PyObjectTemporary high_temp( TO_RANGE_ARG( high, "end" ) );
    PyObjectTemporary step_temp( TO_RANGE_ARG( step, "step" ) );

    bool fallback = false;

    long start = PyInt_AsLong( low_temp.asObject() );

    if (unlikely( start == -1 && PyErr_Occurred() ))
    {
        PyErr_Clear();
        fallback = true;
    }

    long end = PyInt_AsLong( high_temp.asObject() );

    if (unlikely( end == -1 && PyErr_Occurred() ))
    {
        PyErr_Clear();
        fallback = true;
    }

    long step_long = PyInt_AsLong( step_temp.asObject() );

    if (unlikely( step_long == -1 && PyErr_Occurred() ))
    {
        PyErr_Clear();
        fallback = true;
    }

    if ( fallback )
    {
        return _python_builtin_range.call( step_temp.asObject(), high_temp.asObject(), low_temp.asObject() );
    }
    else
    {
        if (unlikely( step_long == 0 ))
        {
            PyErr_Format( PyExc_ValueError, "range() step argument must not be zero" );
            throw _PythonException();
        }

        return BUILTIN_RANGE( start, end, step_long );
    }
}

PyObject *BUILTIN_LEN( PyObject *value )
{
    Py_ssize_t res = PyObject_Size( value );

    if (unlikely( res < 0 && PyErr_Occurred() ))
    {
        throw _PythonException();
    }

    return PyInt_FromSsize_t( res );
}

static PyObject *empty_code = PyBuffer_FromMemory( NULL, 0 );

static PyCodeObject *MAKE_CODEOBJ( PyObject *filename, PyObject *function_name, int line )
{
    // TODO: Potentially it is possible to create a line2no table that will allow to use
    // only one code object per function, this could then be cached and presumably be much
    // faster, because it could be reused.

    assert( PyString_Check( filename ) );
    assert( PyString_Check( function_name ) );

    assert( empty_code );

    // printf( "MAKE_CODEOBJ code object %d\n", empty_code->ob_refcnt );

    PyCodeObject *result = PyCode_New (
        0, 0, 0, 0, // argcount, locals, stacksize, flags
        empty_code, //
        _python_tuple_empty,
        _python_tuple_empty,
        _python_tuple_empty,
        _python_tuple_empty,
        _python_tuple_empty,
        filename,
        function_name,
        line,
        _python_str_empty
    );

    if (unlikely( result == NULL ))
    {
        throw _PythonException();
    }

    return result;
}

PyObject *MAKE_FRAME( PyObject *module, PyObject *filename, PyObject *function_name, int line )
{
    PyCodeObject *code = MAKE_CODEOBJ( filename, function_name, line );

    PyFrameObject *result = PyFrame_New(
        PyThreadState_GET(),
        code,
        ((PyModuleObject *)module)->md_dict,
        NULL // No locals yet
    );

    Py_DECREF( code );

    if (unlikely( result == NULL ))
    {
        throw _PythonException();
    }

    result->f_lineno = line;

    return (PyObject *)result;
}

#ifdef _NUITKA_EXE
extern bool FIND_EMBEDDED_MODULE( char const *name );
#endif

PyObject *IMPORT_MODULE( PyObject *module_name, PyObject *import_name, PyObject *import_items )
{

#ifdef _NUITKA_EXE
    // First try our own package resistent form of frozen modules if we have them
    // embedded. And avoid recursion here too, in case of cyclic dependencies.
    if ( !HAS_KEY( PySys_GetObject( (char *)"modules" ), module_name ) )
    {
        if ( FIND_EMBEDDED_MODULE( PyString_AsString( module_name ) ) )
        {
            return LOOKUP_SUBSCRIPT( PySys_GetObject( (char *)"modules" ), import_name );
        }
    }
#endif

    int line = _current_line;
    PyObject *result = PyImport_ImportModuleEx( PyString_AsString( module_name ), NULL, NULL, import_items );
    _current_line = line;

    if (unlikely( result == NULL ))
    {
        throw _PythonException();
    }

    // Release the reference returned from the import, we don't trust it, because it
    // doesn't work well with packages. Look up in sys.modules instead.
    Py_DECREF( result );

    return LOOKUP_SUBSCRIPT( PySys_GetObject( (char *)"modules" ), import_name );
}

void IMPORT_MODULE_STAR( PyObject *target, bool is_module, PyObject *module_name )
{
    PyObject *module = IMPORT_MODULE( module_name, module_name, NULL );

    // IMPORT_MODULE would raise exception already
    assert( module != NULL );

    PyObject *iter;
    bool all_case;

    if ( PyObject *all = PyMapping_GetItemString( module, (char *)"__all__" ) )
    {
        iter = MAKE_ITERATOR( all );
        all_case = true;
    }
    else
    {
        PyErr_Clear();

        iter = MAKE_ITERATOR( PyModule_GetDict( module ) );
        all_case = false;
    }

    while ( PyObject *item = PyIter_Next( iter ) )
    {
        assert( PyString_Check( item ) );

        // TODO: Not yet clear, what happens with __all__ and "_" of its contents.
        if ( all_case == false )
        {
            if ( PyString_AS_STRING( item )[0] == '_' )
            {
                continue;
            }
        }

        // TODO: Check if the reference is handled correctly
        if ( is_module )
        {
            SET_ATTRIBUTE( target, item, LOOKUP_ATTRIBUTE( module, item ) );
        }
        else
        {
            SET_SUBSCRIPT( target, item, LOOKUP_ATTRIBUTE( module, item ) );
        }

        Py_DECREF( item );
    }

    if ( PyErr_Occurred() )
    {
        throw _PythonException();
    }
}

// Helper functions for print. Need to play nice with Python softspace behaviour.

#if PY_MAJOR_VERSION < 3

void PRINT_ITEM_TO( PyObject *file, PyObject *object )
{
    PyObject *str = PyObject_Str( object );
    PyObject *print;
    bool softspace;

    if ( str == NULL )
    {
        PyErr_Clear();

        print = object;
        softspace = false;
    }
    else
    {
        char *buffer;
        Py_ssize_t length;

#ifndef __NUITKA_NO_ASSERT__
        int status =
#endif
            PyString_AsStringAndSize( str, &buffer, &length );
        assert( status != -1 );

        softspace = length > 0 && buffer[length - 1 ] == '\t';

        print = str;
    }

    // Check for soft space indicator, need to hold a reference to the file
    // or else __getattr__ may release "file" in the mean time.
    if ( PyFile_SoftSpace( file, !softspace ) )
    {
        if (unlikely( PyFile_WriteString( " ", file ) == -1 ))
        {
            Py_DECREF( file );
            Py_DECREF( str );
            throw _PythonException();
        }
    }

    if ( unlikely( PyFile_WriteObject( print, file, Py_PRINT_RAW ) == -1 ))
    {
        Py_XDECREF( str );
        throw _PythonException();
    }

    Py_XDECREF( str );

    if ( softspace )
    {
        PyFile_SoftSpace( file, !softspace );
    }
}

void PRINT_NEW_LINE_TO( PyObject *file )
{
    if (unlikely( PyFile_WriteString( "\n", file ) == -1))
    {
        throw _PythonException();
    }

    PyFile_SoftSpace( file, 0 );
}

#endif

PyObject *GET_STDOUT()
{
    PyObject *stdout = PySys_GetObject( (char *)"stdout" );

    if (unlikely( stdout == NULL ))
    {
        PyErr_Format( PyExc_RuntimeError, "lost sys.stdout" );
        throw _PythonException();
    }

    return stdout;
}

#if PY_MAJOR_VERSION < 3

void PRINT_NEW_LINE( void )
{
    PRINT_NEW_LINE_TO( GET_STDOUT() );
}

#endif

// We unstream some constant objects using the "cPickle" module function "loads"
static PyObject *_module_cPickle = NULL;
static PyObject *_module_cPickle_function_loads = NULL;

void UNSTREAM_INIT( void )
{
#if PY_MAJOR_VERSION < 3
        _module_cPickle = PyImport_ImportModule( "cPickle" );
#else
        _module_cPickle = PyImport_ImportModule( "pickle" );
#endif
        assert( _module_cPickle );

        _module_cPickle_function_loads = PyObject_GetAttrString( _module_cPickle, "loads" );
        assert( _module_cPickle_function_loads );
}

PyObject *UNSTREAM_CONSTANT( char const *buffer, int size )
{
    PyObjectTemporary temp_str( PyString_FromStringAndSize( buffer, size ) );

    PyObject *result = PyObject_CallFunctionObjArgs(
        _module_cPickle_function_loads,
        temp_str.asObject(),
        NULL
    );

    assert( result );

    return result;
}