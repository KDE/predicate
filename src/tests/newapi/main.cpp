#include <qfileinfo.h>

#include <kdebug.h>
#include <kinstance.h>

#include <kexidb/drivermanager.h>
#include <kexidb/driver.h>
#include <kexidb/connection.h>
#include <kexidb/cursor.h>

void usage(char *a)
{
	kdDebug() << "usage: " << a << " <driver_name>" << endl;
}

int main(int argc, char* argv[])
{
	KInstance instance( QFileInfo(argv[0]).baseName().latin1() );
	if (argc<=1) {
		usage(argv[0]);
		return 0;
	}
	QCString drv_name(argv[1]);

	KexiDB::DriverManager manager; // = KexiDB::DriverManager::self();
	QStringList names = manager.driverNames();
	kdDebug() << "DRIVERS: " << endl;
	for (QStringList::iterator it = names.begin(); it != names.end() ; ++it)
		kdDebug() << *it << endl;
	if (manager.error()) {
		kdDebug() << manager.errorMsg() << endl;
		return 1;
	}

	//get driver
	KexiDB::Driver *driver = manager.driver(drv_name);
	if (manager.error()) {
		kdDebug() << manager.errorMsg() << endl;
		return 1;
	}

	//connection data that can be later reused
	KexiDB::ConnectionData conn_data;
//	conn_data.host = "myhost";
//	conn_data.password = "mypwd";
#if 0
	conn_data.setFileName( "db" );

	KexiDB::Connection *conn = driver->createConnection(conn_data);
	if (driver->error()) {
		kdDebug() << driver->errorMsg() << endl;
		return 1;
	}
	if (!conn->connect()) {
		kdDebug() << conn->errorMsg() << endl;
		return 1;
	}
	kdDebug() << "DATABASES:" << conn->databaseNames() << endl;
	if (conn->error()) {
		kdDebug() << conn->errorMsg() << endl;
		return 1;
	}
	if (conn->databaseExists( "db" )) {
		debug("dropDatabase()=%d",conn->dropDatabase());
	}
	debug("createDatabase()=%d",conn->createDatabase("db"));

#endif

#if 1
	conn_data.setFileName( "mydb" );

	KexiDB::Connection *conn = driver->createConnection(conn_data);
	if (driver->error()) {
		kdDebug() << driver->errorMsg() << endl;
		return 1;
	}
	if (!conn->connect()) {
		kdDebug() << conn->errorMsg() << endl;
		return 1;
	}
	if (!conn->databaseExists( "mydb" )) {
		if (!conn->createDatabase( "mydb" )) {
			kdDebug() << conn->errorMsg() << endl;
			return 1;
		}
		kdDebug() << "DB created"<< endl;
	}
	if (!conn->useDatabase( "mydb" )) {
		kdDebug() << conn->errorMsg() << endl;
		return 1;
	}
	KexiDB::Cursor *cursor = conn->executeQuery( "select * from osoby", KexiDB::Cursor::Buffered );
	debug("executeQuery() = %d",!!cursor);
	if (cursor) {
		debug("Cursor::moveLast() == %d",cursor->moveLast());
		debug("Cursor::moveFirst() == %d",cursor->moveFirst());

		debug("Cursor::moveNext() == %d",cursor->moveNext());
		debug("Cursor::moveNext() == %d",cursor->moveNext());
		debug("Cursor::moveNext() == %d",cursor->moveNext());
		debug("Cursor::moveNext() == %d",cursor->moveNext());
		debug("Cursor::eof() == %d",cursor->eof());
		conn->deleteCursor(cursor);
//		delete cursor;
	}
#endif

#if 0
	conn_data.setFileName( "db" );

	KexiDB::Connection *conn = driver->createConnection(conn_data);
	if (driver->error()) {
		kdDebug() << driver->errorMsg() << endl;
		return 1;
	}
	if (!conn->connect()) {
		kdDebug() << conn->errorMsg() << endl;
		return 1;
	}
	if (!conn->useDatabase( "db" )) {
		kdDebug() << conn->errorMsg() << endl;
		return 1;
	}

	KexiDB::Table *t = conn->tableSchema( "persons" );
	if (t)
		t->debug();
	t = conn->tableSchema( "cars" );
	if (t)
		t->debug();

#endif
//	conn->tableNames();
/*
	if (!conn->disconnect()) {
		kdDebug() << conn->errorMsg() << endl;
		return 1;
	}*/
//	debug("before del");
//	delete conn;
//	debug("after del");
	return 0;
}
