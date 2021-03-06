add_feature_info(BUILD_POSTGRESQL_DB_DRIVER TRUE ${BUILD_POSTGRESQL_DB_DRIVER_DESC})

include_directories(
  ${PostgreSQL_INCLUDE_DIRS}
)

ADD_DEFINITIONS(
  -D__PREDICATE__=
  -DPREDICATE_DRIVER_NAME="postgresql"
)

########### next target ###############

set(predicate_postgresql_SRCS
   PostgresqlDriver.cpp
   PostgresqlConnection.cpp
   PostgresqlCursor.cpp
   PostgresqlKeywords.cpp
   PostgresqlConnection_p.cpp
   PostgresqlPreparedStatement.cpp
)

set(predicate_postgresql_MOC_SRCS
   PostgresqlDriver.h
)

qt_wrap_cpp(predicate_postgresql predicate_postgresql_SRCS ${predicate_postgresql_MOC_SRCS})

add_library(predicate_postgresql SHARED ${predicate_postgresql_SRCS})

target_link_libraries(predicate_postgresql
  ${PostgreSQL_LIBRARIES}
  predicate
  ${QT_QTNETWORK_LIBRARY}
)
set_target_properties(predicate_postgresql PROPERTIES PREFIX "")

install(TARGETS predicate_postgresql DESTINATION ${PREDICATE_PLUGINS_DIR})
install(FILES predicate_postgresql.desktop DESTINATION ${PREDICATE_PLUGINS_DIR})
