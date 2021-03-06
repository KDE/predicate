/* This file is part of the KDE project
   Copyright (C) 2006 Jarosław Staniek <staniek@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#ifndef PREDICATE_SIMPLECMDLINEAPP_H
#define PREDICATE_SIMPLECMDLINEAPP_H

#include <Predicate/Connection.h>
#include <Predicate/Driver.h>

#include <KAboutData>

class KCmdLineOptions;
class KComponentData;

namespace Predicate
{
//! @short A skeleton for creating a simple command line database application.
/*! This class creates a KComponentData object and automatically handles the following
 command line options:
 - --driver \<name\> (Database driver name) or -drv
 - --user \<name\> (Database user name) or -u
 - --password (Prompt for password) or -p
 - --host \<name\> (Server (host) name) or -h
 - --port \<number\> (Server's port number)
 - --local-socket \<filename\> (Server's local socket filename, if needed) or -s

 You can use this helper class to create test applications or small tools that open
 a Predicate-compatible database using command line arguments, do some data processing
 and close the database.
*/
class PREDICATE_EXPORT SimpleCommandLineApp : public Predicate::Object
{
public:
    SimpleCommandLineApp(
        int argc, char** argv,
        const KCmdLineOptions &options, const char *programName,
        const char *version, const char *shortDescription = 0,
        KAboutData::LicenseKey licenseType = KAboutData::License_Unknown,
        const char *copyrightStatement = 0, const char *text = 0,
        const char *homePageAddress = 0, const char *bugsEmailAddress = "submit@bugs.kde.org");

    ~SimpleCommandLineApp();

    //! @return program instance
    const KComponentData &componentData() const;

    /*! Opens database @a databaseName for connection data
     specified via the command line. @return true in success.
     In details: the database driver is loaded, the connection is opened
     and the database is used.
     Use Predicate::Object methods to get status of the operation on failure. */
    bool openDatabase(const QString& databaseName);

    /*! Closes database connection previously opened using openDatabase()
     @return true on success. This method is called on destruction.
     Use Predicate::Object methods to get status of the operation on failure. */
    bool closeDatabase();

    /*! @return connection data for this application. */
    Predicate::ConnectionData* connectionData() const;

    /*! @return connection object for this application or 0 if there is no properly
     opened connection. */
    Predicate::Connection* connection() const;

protected:
    class Private;
    Private * const d;
};
}

#endif
