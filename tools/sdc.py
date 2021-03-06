#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#   This file is part of the KDE project
#   Copyright (C) 2010 Jarosław Staniek <staniek@kde.org>
#
#   This program is free software; you can redistribute it and/or
#   modify it under the terms of the GNU General Public
#   License as published by the Free Software Foundation; either
#   version 2 of the License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Library General Public License for more details.
#
#   You should have received a copy of the GNU Library General Public License
#   along with this program; see the file COPYING.  If not, write to
#   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
#   Boston, MA 02110-1301, USA.

#
# Shared Data Compiler
#

import os, sys, shlex

version = '0.1'

def usage():
    print '''Usage: %s [INPUT] [OUTPUT]
Shared Data Compiler version %s
''' % (sys.argv[0], version)

def syntax_error(msg):
    print "Syntax error in %s: %s" % (in_fname, msg)
    sys.exit(1)

if len(sys.argv) < 3:
    usage()
    sys.exit(0)

# --- open ---
in_fname = sys.argv[1]
out_fname = sys.argv[2]

try:
    infile = open(in_fname, "r")
    outfile = open(out_fname, "w")
except Exception, inst:
    print inst
    sys.exit(1)

outfile_sdc = None

# --- utils ---
def param(lst, name):
    for item in lst:
        s = item.split('=')
        if len(s) > 1 and s[0] == name:
            return s[1]
    return ''

def param_exists(lst, name):
    try:
        if lst.index(name) >= 0:
            return True
    except ValueError:
            pass
    return False


# --- process ---
shared_class_name = ''
shared_class_options = {}
generated_code_inserted = False
shared_class_inserted = False
data_class_ctor = ''
data_class_copy_ctor = ''
data_class_members = ''
members_list = []
data_accesors = ''
protected_data_accesors = ''
main_ctor = ''
member = {}
toMap_impl = ''
fromMap_impl = ''

def get_file(fname):
    if fname.rfind('/') == -1:
        return fname
    return fname[fname.rfind('/')+1:]

def open_sdc():
    global outfile_sdc, shared_class_options
    if not outfile_sdc:
        sdc_fname = out_fname.replace('.h', '_sdc.cpp')
        try:
            outfile_sdc = open(sdc_fname, "w")
        except Exception, inst:
            print inst
            sys.exit(1)
    outfile_sdc.write(warningHeader())
    if not shared_class_options['namespace']:
        syntax_error("Shared class option \"with_from_to_map\" needs \"namespace\" option")
    outfile_sdc.write("""#include "%s"
#include <QVariant>

using namespace %s;

""" % (get_file(out_fname), shared_class_options['namespace']))

"""
    Inserts generated fromMap(), toMap() code into the output.
    Declarations are inserted into the header, definitions into extra *_sdc.cpp file
"""
def insert_fromMap_toMap_methods():
    global outfile, shared_class_name, toMap_impl, fromMap_impl
    outfile.write("""    /*! @return map with saved attributes of the %s object. @see fromMap(). */
    QMap<QString, QString> toMap() const {
        return d->toMap();
    }

""" % shared_class_name)
    open_sdc()
    outfile_sdc.write("""%s::Data::Data(const QMap<QString, QString>& map, bool *ok)
{
%s
    if (ok)
        *ok = true;
}

QMap<QString, QString> %s::Data::toMap() const
{
    QMap<QString, QString> map;
%s
    return map;
}
""" % (shared_class_name, fromMap_impl, shared_class_name, toMap_impl))
    outfile_sdc.close()

"""
    Inserts generated operator==() code for shared class into the output.
"""
def insert_operator_eq():
    global outfile, shared_class_name
    outfile.write("""    bool operator==(const %s& other) const {
        return *d == *other.d;
    }
""" % shared_class_name)

"""
    Inserts generated Data::operator==() code into the output.
"""
def insert_data_operator_eq():
    global outfile, members_list
    outfile.write("""        bool operator==(const Data& other) const {
""")
    outfile.write("            return ")
    first = True;
    space = ""
    for member in members_list:
        outfile.write("""%s%s == other.%s""" % (space, member, member))
        if first:
            first = False
            space = """
                && """
    outfile.write(""";
        }

""")

"""
    Inserts generated code into the output.
"""
def insert_generated_code():
    global infile, outfile, generated_code_inserted, data_class_ctor, data_class_copy_ctor, data_class_members, data_accesors, protected_data_accesors, main_ctor, shared_class_name, shared_class_options
    if generated_code_inserted:
        return;
    #print "--------insert_generated_code--------"
    outfile.write(data_class_ctor)
    outfile.write("""        {
        }
""")
    outfile.write(data_class_copy_ctor)
    outfile.write("""        {
        }

""")
    if shared_class_options['with_from_to_map']:
        outfile.write("""        /*! Constructor for Data object, takes attributes saved to map @a map.
        If @a ok is not null, sets *ok to true on success and to false on failure. @see toMap(). */
        Data(const QMap<QString, QString>& map, bool *ok);

        QMap<QString, QString> toMap() const;

""")
    if shared_class_options['operator==']:
        insert_data_operator_eq()

    outfile.write(data_class_members)
    outfile.write(main_ctor)
    outfile.write(data_accesors)
    outfile.write("\n")
    if shared_class_options['with_from_to_map']:
        insert_fromMap_toMap_methods()
    if shared_class_options['operator==']:
        insert_operator_eq()
    if protected_data_accesors:
        outfile.write("protected:")
        outfile.write(protected_data_accesors)
        outfile.write("\npublic:")
    outfile.write("\n")
    generated_code_inserted = True


"""
    Reads documentation for single section (setter or getter) and returns it.
    Leaves the file pointer before */ or another @getter/@setter mark.
"""
def read_getter_or_setter_doc():
    result = ''
    while True:
        prev_pos = infile.tell()
        line = infile.readline()
        if not line:
            break
        elif line.find('*/') != -1 or line.find('@getter') != -1 or line.find('@setter') != -1:
            #print "seek prev from " + line
            infile.seek(prev_pos)
            break
        else:
            result += line
    return result

def process_docs(comment):
    result = {}
    while True:
        line = infile.readline()
        #print "process_docs: " + line
        if not line:
            break
        elif line.find('*/') != -1:
            if result == {}:
                insert_generated_code()
                outfile.write(line)
            break
        elif line.find('@getter') != -1:
            result['getter'] = read_getter_or_setter_doc()
        elif line.find('@setter') != -1:
            result['setter'] = read_getter_or_setter_doc()
        else:
            insert_generated_code()
            outfile.write(comment)
            outfile.write(line)
    if result == {}:
        result = None
    #print "process_docs result: " + str(result)
    return result

def try_read_member_docs(comment):
    prev_pos = infile.tell()
    result = comment
    while True:
        line = infile.readline()
        if not line or line.find('@getter') != -1 or line.find('@setter') != -1:
            infile.seek(prev_pos)
            return None
        elif line.find('*/') != -1:
            return result
        else:
            result += line
    return None

""" makes setter out of name or returns forceSetter is specified """
def makeSetter(name, forceSetter):
    if forceSetter:
        return forceSetter
    return 'set' + name[0].upper() + name[1:]

def update_data_accesors():
    global data_accesors, protected_data_accesors, member
    if not member['no_getter']:
        if member.has_key('getter_docs'):
            val = '\n    /*!\n' + member['getter_docs'] + '    */'
            if member['access'] == 'public':
                data_accesors += val
            else: # protected
                protected_data_accesors += val
        getter = member['getter']
        if not getter:
            getter = member['name']
        val = """
    %s %s() const {
        return d->%s;
    }
""" % (member['type'], getter, member['name'])
        if member['access'] == 'public':
            data_accesors += val
        else: # protected
            protected_data_accesors += val
    if not member['no_setter']:
        if member.has_key('setter_docs'):
            val = '\n    /*!\n' + member['setter_docs'] + '    */'
            if member['access'] == 'public':
                data_accesors += val
            else: # protected
                protected_data_accesors += val
        # heuristics to check if the const & should be used:
        arg_type = member['type']
        if arg_type.lower() != arg_type:
            arg_type = 'const %s&' % arg_type
        setter = makeSetter(member['name'], member['setter'])
        default_setter = ''
        if member['default_setter']:
            default_setter = ' = ' + member['default_setter']
        if member['custom_setter']:
            val = """
    void %s(%s %s%s);
""" % (setter, arg_type, member['name'], default_setter)
        else:
            val = """
    void %s(%s %s%s) {
        d->%s = %s;
    }
""" % (setter, arg_type, member['name'], default_setter, member['name'], member['name'])
        if member['access'] == 'public':
            data_accesors += val
        else: # protected
            protected_data_accesors += val

def data_member_found(lst):
    if len(lst) > 1 and lst[0] == 'data_member':
        return True
    if len(lst) > 2 and lst[0] == 'protected' and lst[1] == 'data_member':
        return True
    if len(lst) > 1 and lst[0] == 'data_method':
        return True
    return False

# sets shared_class_options[option_name] to proper value; returns lst with removed element option_name if exists
def get_shared_class_option(lst, option_name):
    global shared_class_options
    shared_class_options[option_name] = param_exists(lst, option_name)
    if shared_class_options[option_name]:
        lst.remove(option_name)
    return lst

# like get_shared_class_option() but also gets value (not just checks for existence)
def get_shared_class_option_with_value(lst, option_name):
    global shared_class_options
    for item in lst:
        s = item.split('=')
        if len(s) > 1 and s[0] == option_name:
            lst.remove(item)
            shared_class_options[option_name] = s[1]
            return lst
    shared_class_options[option_name] = False
    return lst

def warningHeader():
    return """/****************************************************************************
** Implicitly Shared Class code from reading file '%s'
**
** Created
**      by: The Shared Data Compiler version %s
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

""" % (get_file(in_fname), version)

# generates conversion code to string from many types, used by Data::toMap()
# @todo more types
def generate_toString_conversion(name, _type):
    if _type == 'QString' or _type == 'QByteArray':
        return name
    elif _type == 'bool':
        return 'QString::number((int)%s)' % name # 0 or 1
    return 'QVariant(%s).toString()' % name

# generates conversion code from string to many types, used by Data(QMap<..>)
# @todo more types
def generate_fromString_conversion(name, _type):
    s = 'map[QLatin1String(\"%s\")]' % name
    if _type == 'bool': # 0 or 1
        return """%s = %s.toInt(ok) == 1;
    if (ok && !(*ok))
        return;
""" % (name, s)
    elif _type == 'int':
        return """%s = %s.toInt(ok);
    if (ok && !(*ok))
        return;
""" % (name, s)
    else: # QString...
        return "%s = %s;" % (name, s)

# returns position (line number) for #include <QSharedData> or -1 if #include <QSharedData> isn't needed
def get_pos_for_QSharedData_h():
    global infile
    prev_pos = infile.tell()
    line_number = -1
    infile.seek(0)
    # find last #include
    last_include = -1
    while True:
        line = infile.readline().lower()
        if not line:
            break
        line_number += 1
        if line.find('#include ') != -1:
            if line.find('qshareddata') != -1:
                last_include = -1
                break
            else:
                last_include = line_number + 1
    infile.seek(prev_pos)
    return last_include

# replaces "Foo<ABC<DEF>>" with "Foo< ABC< DEF > >" to avoid build errors
def fix_templates(s):
    result=''
    for c in s:
        if c == '>':
            result += ' '
        result += c
        if c == '<':
            result += ' '
    return result

def other_comment(line):
    ln = line.strip(' ')
    return ln.startswith('/**') \
      or ln.startswith('/*!') \
      or ln .startswith('//!') \
      or ln.startswith('///')
    
def process():
    global infile, outfile, generated_code_inserted, data_class_ctor, data_class_copy_ctor, shared_class_name, shared_class_options, shared_class_inserted, data_class_members, members_list, data_accesors, member, main_ctor, toMap_impl, fromMap_impl
    outfile.write(warningHeader())

    member = {}
    after_member = False

    data_class_ctor = ''
    data_class_copy_ctor = ''
    data_class_ctor_changed = False
    data_class_copy_ctor_changed = False
    position_for_include_QSharedData_h = get_pos_for_QSharedData_h()

    line_number = -1 # used for writing #include <QSharedData>
    while True:
        line = infile.readline()
        line_number += 1
        if not line:
            break
#        print line,
        lst = line.split()
#        print lst
        if line_number == position_for_include_QSharedData_h:
            outfile.write("""#include <QSharedData>
""")
            position_for_include_QSharedData_h = -1
        if line.startswith('class'):
            shared_class_inserted = False # required because otherwise QSharedDataPointer<Data> d will be added to all classes
            outfile.write(line)
        elif line.startswith('shared class'):
            # re-init variables, needed if there are more than one shared class per file
            shared_class_inserted = False
            generated_code_inserted = False
            shared_class_options = {}
            data_class_ctor = ''
            data_class_copy_ctor = ''
            data_class_members = ''
            members_list = []
            data_accesors = ''
            protected_data_accesors = ''
            toMap_impl = ''
            fromMap_impl = ''
            data_class_ctor_changed = False
            data_class_copy_ctor_changed = False
            lst = shlex.split(line) # better than default split()
            # syntax: shared class export=<EXPORT> inherits=<INHERITANCE> <NAME>
            # INHERITANCE is e.g. inherits="public Foo" - use quotes
            # output: class <EXPORT> <NAME>
            export = param(lst, 'export')
            inherits = param(lst, 'inherits')
            lst = get_shared_class_option(lst, 'operator==')
            lst = get_shared_class_option(lst, 'with_from_to_map')
            lst = get_shared_class_option(lst, 'virtual_dtor')
            lst = get_shared_class_option_with_value(lst, 'namespace')
            shared_class_name = lst[-1]
            main_ctor = """    };

    %s()
     : d(new Data)
    {
    }

    %s(const %s& other)
     : d(other.d)
    {
    }
""" % (shared_class_name, shared_class_name, shared_class_name)
            if shared_class_options['with_from_to_map']:
                main_ctor += """    /*! Constructor for %s object, takes attributes saved to map @a map.
         If @a ok is not null, sets *ok to true on success and to false on failure. @see toMap(). */
    %s(const QMap<QString, QString>& map, bool *ok)
     : d(new Data(map, ok))
    {
    }
""" % (shared_class_name, shared_class_name)
            main_ctor += """
    %s~%s();
""" % (('virtual ' if shared_class_options['virtual_dtor'] else ''), shared_class_name)
            if export:
                name = export + ' ' + shared_class_name
            if inherits:
                inherits = ' : ' + inherits
            outfile.write("class %s%s\n" % (name, inherits))
            line = infile.readline() # {
            outfile.write(line)
            line = infile.readline() # public:
            outfile.write(line)
            shared_class_inserted = True
        elif len(lst) >= 2 and lst[0] == '#if' and lst[1] == '0':
            insert_generated_code()
            outfile.write(line)
            while True:
                line = infile.readline()
                lst = line.split()
                if not line:
                    break
                elif len(lst) >= 1 and lst[0] == '#endif':
                    outfile.write(line)
                    break
                outfile.write(line)
        elif len(lst) == 1 and (lst[0] == '/**' or lst[0] == '/*!'):
            comment = line

            member_docs = try_read_member_docs(comment)
            #print "member_docs:" + str(member_docs)

            docs = None
            if member_docs:
                member = {}
                member['docs'] = '\n    ' + member_docs.replace('\n', '\n    ') + '    */';
            else:
                docs = process_docs(comment)
            if docs:
                #print "DOCS:" + str(docs)
                member = {}
                if docs.has_key('getter'):
                    member['getter_docs'] = docs['getter']
                if docs.has_key('setter'):
                    member['setter_docs'] = docs['setter']
            elif not member_docs:
                insert_generated_code()
                outfile.write(comment)
        elif data_member_found(lst):
            if lst[-1].endswith(';'):
                lst[-1] = lst[-1][:-1]
            #print lst
            # syntax: data_member <TYPE> <NAME> [default=<DEFAULT_VAL>] [default_setter=<DEFAULT_SETTER_VAL>]
            # output: getter, setter methods, data memeber
            if lst[0] == 'data_method':
                #if member.has_key('docs'):
                #    data_class_members += member['docs'] + '\n'
                #    del member['docs']
                data_class_members += "        %s;\n" % (' '.join(lst[1:]))
                continue
            elif lst[0] == 'protected':
                member['access'] = 'protected'
                lst = lst[1:]
            else:
                member['access'] = 'public'
            member['type'] = fix_templates(lst[1])
            member['name'] = lst[2]
            members_list.append(member['name']);
            member['default'] = param(lst, 'default')
            member['default_setter'] = param(lst, 'default_setter')
            member['no_getter'] = param_exists(lst, 'no_getter')
            member['getter'] = param(lst, 'getter')
            member['no_setter'] = param_exists(lst, 'no_setter')
            member['setter'] = param(lst, 'setter')
            member['custom_setter'] = param_exists(lst, 'custom_setter')
            member['mutable'] = param_exists(lst, 'mutable')
            #print member
            if not data_class_ctor_changed:
                data_class_ctor = """    //! Internal data class used to implement implicitly shared class %s.\n    //! Provides thread-safe reference counting.
    class Data : public QSharedData
    {
    public:
        Data()
""" % shared_class_name
            if not data_class_copy_ctor_changed:
                data_class_copy_ctor = """
        Data(const Data& other)
        : QSharedData(other)
"""
                data_class_copy_ctor_changed = True
            if member['default']:
                data_class_ctor += '        '
                if data_class_ctor_changed:
                    data_class_ctor += ', '
                else:
                    data_class_ctor += ': '
                    data_class_ctor_changed = True
                data_class_ctor += member['name'] + '(' + member['default'] + ')\n'
#            print data_class_ctor
            data_class_copy_ctor += '        , %s(other.%s)\n' % (member['name'], member['name'])
            if member.has_key('docs'):
                data_class_members += member['docs']
            if not member['no_getter'] or not member['no_setter']:
                data_class_members += "        //! @see "
                if not member['no_getter']:
                    getter = member['getter']
                    if not getter:
                        getter = member['name']
                    data_class_members += "%s::%s()" % (shared_class_name, getter)
                if member['no_setter']:
                    data_class_members += "\n"
                else:
                    if not member['no_getter']:
                        data_class_members += ", "
                    setter = makeSetter(member['name'], member['setter'])
                    data_class_members += "%s::%s()\n" % (shared_class_name, setter)
            data_class_members += "        %s%s %s;\n" % (('mutable ' if member['mutable'] else ''), member['type'], member['name'])
            if shared_class_options['with_from_to_map']:
                toMap_impl += '    map[QLatin1String(\"%s\")] = %s;\n' % (member['name'], generate_toString_conversion(member['name'], member['type']))
                fromMap_impl += '    %s\n' % generate_fromString_conversion(member['name'], member['type'])
            update_data_accesors()
            member = {}
            after_member = True
        elif len(lst) > 0 and lst[0] == '};' and line[:2] == '};' and shared_class_inserted:
            insert_generated_code()
#            outfile.write('\nprivate:\n');
            outfile.write('\nprotected:\n');
            outfile.write('    QSharedDataPointer<Data> d;\n');
            outfile.write(line)
        else:
            if False and other_comment(line):
                prev_pos = infile.tell()
                prev_line_number = line_number
                ln = line[:-1].strip(' ')
                result = ''
                print "'" + ln + "'"
                if ln.startswith('/**') or ln.startswith('/*!'):
                    while True:
                        result += line
                        if not line or ln.endswith('*/'):
                            result = result[:-1]
                            break
                        line = infile.readline()
                        line_number += 1
                        ln = line[:-1].strip(' ')
                    print "!!"
                    print result
                    print "!!"
                if result:
                    member['docs'] = result
                infile.seek(prev_pos)
                line = infile.readline()
                lst = line.split()
                line_number = prev_line_number

            if not after_member or len(lst) > 0:
                if shared_class_inserted:
                    insert_generated_code()
                outfile.write(line)
            elif generated_code_inserted and len(lst) == 0:
                outfile.write(line)
#            else:
#                outfile.write(line)

process()

# --- close ---
infile.close()
outfile.close()
