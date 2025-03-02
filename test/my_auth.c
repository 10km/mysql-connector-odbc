// Copyright (c) 2011, 2021, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0, as
// published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation. The authors of MySQL hereby grant you an
// additional permission to link the program and your derivative works
// with the separately licensed software that they have included with
// MySQL.
//
// Without limiting anything contained in the foregoing, this file,
// which is part of <MySQL Product>, is also subject to the
// Universal FOSS Exception, version 1.0, a copy of which can be found at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

#include "odbctap.h"
#include "mysql_version.h"

/*
  Test for setting plugin-dir and default-auth mysql options
  using Connector/ODBC connection parameter.

*/
DECLARE_TEST(t_plugin_auth)
{
#if MYSQL_VERSION_ID >= 50507
  SQLCHAR   conn[512], conn_out[512];
  SQLSMALLINT conn_out_len;
  SQLCHAR *tplugin_dir= (SQLCHAR *)"/tmp/test_new_directory/";
  SQLCHAR *tdefault_auth= (SQLCHAR *)"auth_test_plugin";
  HDBC hdbc1;
  HSTMT hstmt1;
  SQLCHAR buf[255];
  SQLLEN buflen;
  SQLRETURN rc;
  SQLUSMALLINT plugin_status= FALSE;

  ok_sql(hstmt, "SELECT PLUGIN_NAME FROM INFORMATION_SCHEMA.PLUGINS "
                "WHERE PLUGIN_NAME='test_plugin_server';");
  SQLFetch(hstmt);
  SQLGetData(hstmt, 1, SQL_CHAR, buf, sizeof(buf), &buflen);

  /* Check whether plugin already exist, if not install it. */
  if(strcmp(buf, "test_plugin_server")!=0)
  {
    plugin_status= TRUE;
#ifdef _WIN32
    ok_sql(hstmt, "INSTALL PLUGIN test_plugin_server "
                    "SONAME 'auth_test_plugin'");
#else
    ok_sql(hstmt, "INSTALL PLUGIN test_plugin_server "
                    "SONAME 'auth_test_plugin.so'");
#endif
  }

  ok_stmt(hstmt, SQLFreeStmt(hstmt, SQL_CLOSE));

  /* Can FAIL if user does not exists */
  SQLExecDirect(hstmt, "DROP USER `plug_13070711`@`localhost`", SQL_NTS);
  SQLExecDirect(hstmt, "DROP USER `plug_13070711`@`%`", SQL_NTS);

  ok_sql(hstmt, "CREATE USER `plug_13070711`@`%` "
                  "IDENTIFIED BY 'plug_dest_passwd';");
  ok_sql(hstmt, "GRANT ALL PRIVILEGES ON test.* "
                  "TO `plug_13070711`@`%`;");

  ok_sql(hstmt, "CREATE USER `plug_13070711`@`localhost` "
                  "IDENTIFIED BY 'plug_dest_passwd';");
  ok_sql(hstmt, "GRANT ALL PRIVILEGES ON test.* "
                  "TO `plug_13070711`@`localhost`;");

  ok_env(henv, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc1));

  /*
    Test verifies that setting PLUGIN_DIR coonection parameter
    properly works by setting wrong plugin-dir path using
    PLUGIN_DIR connection paramater, which fails connection as
    it won't find required library needed for authentication.
  */
  sprintf((char *)conn, "DSN=%s;UID=plug_13070711;PWD=plug_dest_passwd;"
                        "DATABASE=test;DEFAULT_AUTH=%s;PLUGIN_DIR=%s",
            (char *)mydsn, (char *) tdefault_auth, (char *) tplugin_dir);
  if (mysock != NULL)
  {
    strcat((char *)conn, ";SOCKET=");
    strcat((char *)conn, (char *)mysock);
  }
  if (myport)
  {
    char pbuff[20];
    sprintf(pbuff, ";PORT=%d", myport);
    strcat((char *)conn, pbuff);
  }

  expect_dbc(hdbc1, SQLDriverConnect(hdbc1, NULL, conn, SQL_NTS, conn_out,
                              sizeof(conn_out), &conn_out_len,
                              SQL_DRIVER_NOPROMPT), SQL_ERROR);

  /*
    Test verifies that setting DEFAULT_AUTH coonection parameter
    properly works by setting wrong default-auth library name
    using DEFAULT_AUTH connection paramater, which fails connection as
    it won't find required library needed for authentication.
  */
  sprintf((char *)conn, "DSN=%s;UID=plug_13070711;PWD=plug_dest_passwd;"
                        "DATABASE=test;DEFAULT_AUTH=%s_test",
            (char *)mydsn, (char *) tdefault_auth);
  if (mysock != NULL)
  {
    strcat((char *)conn, ";SOCKET=");
    strcat((char *)conn, (char *)mysock);
  }
  if (myport)
  {
    char pbuff[20];
    sprintf(pbuff, ";PORT=%d", myport);
    strcat((char *)conn, pbuff);
  }

  expect_dbc(hdbc1, SQLDriverConnect(hdbc1, NULL, conn, SQL_NTS, conn_out,
                              sizeof(conn_out), &conn_out_len,
                              SQL_DRIVER_NOPROMPT), SQL_ERROR);

  /*
    Tests for successfull connection using DEFAULT_AUTH and TEST_PLUGINDIR
  */
  sprintf((char *)conn, "DSN=%s;UID=plug_13070711;PWD=plug_dest_passwd;"
                        "DATABASE=test;DEFAULT_AUTH=auth_test_plugin",
  (char *)mydsn);
  if (mysock != NULL)
  {
    strcat((char *)conn, ";SOCKET=");
    strcat((char *)conn, (char *)mysock);
  }
  if (myport)
  {
    char pbuff[20];
    sprintf(pbuff, ";PORT=%d", myport);
    strcat((char *)conn, pbuff);
  }
  if (myauth && myauth[0])
  {
    strcat((char *)conn, ";DEFAULT_AUTH=");
    strcat((char *)conn, (char *)myauth);
  }
  if (myplugindir && myplugindir[0])
  {
    strcat((char *)conn, ";PLUGIN_DIR=");
    strcat((char *)conn, (char *)myplugindir);
  }

  rc= SQLDriverConnect(hdbc1, NULL, conn, SQL_NTS, conn_out,
                        sizeof(conn_out), &conn_out_len,
                        SQL_DRIVER_NOPROMPT);

  /*
    If authetication plugin library 'auth_test_plugin' not found
    then either TEST_PLUGINDIR is not set and/or default path
    does not contain this library.
  */
  if (rc != SQL_SUCCESS)
  {
    ok_sql(hstmt, "DROP USER `plug_13070711`@`localhost`");
    ok_sql(hstmt, "DROP USER `plug_13070711`@`%`");
    ok_sql(hstmt, "UNINSTALL PLUGIN test_plugin_server");
    printf("# Set TEST_PLUGINDIR environment variables\n");
    return FAIL;
  }

  ok_con(hdbc1, SQLAllocStmt(hdbc1, &hstmt1));

  ok_sql(hstmt1, "select CURRENT_USER()");
  ok_stmt(hstmt1,SQLFetch(hstmt1));

  is_str(my_fetch_str(hstmt1, buf, 1), "plug_13070711@localhost", 23);

  ok_sql(hstmt, "DROP USER `plug_13070711`@`localhost`");
  ok_sql(hstmt, "DROP USER `plug_13070711`@`%`");

  if(plugin_status)
  {
    ok_sql(hstmt, "UNINSTALL PLUGIN test_plugin_server");
  }

  ok_stmt(hstmt1, SQLFreeStmt(hstmt1, SQL_DROP));
  ok_con(hdbc1, SQLDisconnect(hdbc1));
  ok_con(hdbc1, SQLFreeHandle(SQL_HANDLE_DBC, hdbc1));
#endif

  return OK;
}

/*
  Test for ldap auth using Connector/ODBC.
*/
DECLARE_TEST(t_ldap_auth)
{
#if MYSQL_VERSION_ID >= 80021
  SQLCHAR   conn[512], conn_out[512];
  SQLSMALLINT conn_out_len;
  HDBC hdbc1;
  HSTMT hstmt1;
  SQLCHAR buf[255];
  SQLLEN buflen;
  SQLRETURN rc;
  const char* ldap_user = getenv("LDAP_USER");
  const char* ldap_user_dn = getenv("LDAP_USER_DN");
  const char* ldap_simple_pwd = getenv("LDAP_SIMPLE_PWD");
  const char* ldap_scram_pwd = getenv("LDAP_SCRAM_PWD");
  const char* plugin_dir = getenv("PLUGIN_DIR");


  //Lets first create ldap and sasl users
  if(ldap_user || ldap_user_dn)
  {
    if(ldap_user_dn)
    {
      sprintf(buf, "CREATE USER IF NOT EXISTS ldap_simple@'%%' IDENTIFIED WITH "
                   "authentication_ldap_simple AS '%s'",ldap_user_dn);
      rc = SQLExecDirect(hstmt, buf, SQL_NTS);
      sprintf(buf, "GRANT ALL ON *.* to ldap_simple@'%%'");
      rc = SQLExecDirect(hstmt,(SQLCHAR*)buf, SQL_NTS);
    }

    if(ldap_user)
    {
      sprintf(buf, "CREATE USER IF NOT EXISTS  %s@'%%' IDENTIFIED WITH authentication_ldap_sasl",
              ldap_user);
      rc = SQLExecDirect(hstmt, buf, SQL_NTS);
      sprintf(buf, "GRANT ALL ON *.* to  %s@'%%'",
              ldap_user);
      rc = SQLExecDirect(hstmt, buf, SQL_NTS);
    }

  }

  if(ldap_simple_pwd)
  {

    ok_env(henv, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc1));

    /*
      Test verifies that setting PLUGIN_DIR coonection parameter
      properly works by setting wrong plugin-dir path using
      PLUGIN_DIR connection paramater, which fails connection as
      it won't find required library needed for authentication.
    */
    sprintf((char *)conn, "DSN=%s;UID=ldap_simple;PWD=%s;"
                          "DATABASE=test; ENABLE_CLEARTEXT_PLUGIN=1",
              (char *)mydsn, ldap_simple_pwd);
    if (mysock != NULL)
    {
      strcat((char *)conn, ";SOCKET=");
      strcat((char *)conn, (char *)mysock);
    }
    if (myport)
    {
      char pbuff[20];
      sprintf(pbuff, ";PORT=%d", myport);
      strcat((char *)conn, pbuff);
    }

    expect_dbc(hdbc1, rc = SQLDriverConnect(hdbc1, NULL, conn, SQL_NTS, conn_out,
                                sizeof(conn_out), &conn_out_len,
                                SQL_DRIVER_NOPROMPT), SQL_DRIVER_NOPROMPT);

    ok_con(hdbc1, SQLAllocHandle(SQL_HANDLE_STMT, hdbc1, &hstmt1));

    rc= SQLExecDirect(hstmt1, (SQLCHAR *)"select 'Hello Simple LDAP'", SQL_NTS);
    if(rc == SQL_SUCCESS)
    {
      SQLFetch(hstmt1);

      SQLGetData(hstmt1, 1, SQL_CHAR, buf, sizeof(buf), &buflen);
      if(buflen > 0)
      {
        printf("OUTPUT: %s\n",buf);
      }
    }

    ok_con(hdbc1, SQLDisconnect(hdbc1));
    ok_con(hdbc1, SQLFreeHandle(SQL_HANDLE_DBC, hdbc1));
  }

  if(ldap_user && ldap_scram_pwd)
  {

    ok_env(henv, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc1));


    /*
      Test verifies that setting PLUGIN_DIR coonection parameter
      properly works by setting wrong plugin-dir path using
      PLUGIN_DIR connection paramater, which fails connection as
      it won't find required library needed for authentication.
    */
    sprintf((char *)conn, "DSN=%s;UID=%s;PWD=%s;"
                          "DATABASE=test; PLUGIN_DIR=%s",
              (char *)mydsn, ldap_user ,ldap_scram_pwd,(char *) plugin_dir);
    if (mysock != NULL)
    {
      strcat((char *)conn, ";SOCKET=");
      strcat((char *)conn, (char *)mysock);
    }
    if (myport)
    {
      char pbuff[20];
      sprintf(pbuff, ";PORT=%d", myport);
      strcat((char *)conn, pbuff);
    }

    expect_dbc(hdbc1, SQLDriverConnect(hdbc1, NULL, conn, SQL_NTS, conn_out,
                                sizeof(conn_out), &conn_out_len,
                                SQL_DRIVER_NOPROMPT), SQL_DRIVER_NOPROMPT);

    ok_con(hdbc1, SQLAllocHandle(SQL_HANDLE_STMT, hdbc1, &hstmt1));

    rc= SQLExecDirect(hstmt1, (SQLCHAR *)"select 'Hello Simple LDAP'", SQL_NTS);
    if(rc == SQL_SUCCESS)
    {
      SQLFetch(hstmt1);

      SQLGetData(hstmt1, 1, SQL_CHAR, buf, sizeof(buf), &buflen);
      if(buflen > 0)
      {
        printf("OUTPUT: %s\n",buf);
      }
    }

    ok_con(hdbc1, SQLDisconnect(hdbc1));
    ok_con(hdbc1, SQLFreeHandle(SQL_HANDLE_DBC, hdbc1));
  }

#endif

  return OK;
}


struct MFA_TEST_DATA
{
  SQLCHAR* user;
  SQLCHAR* pwd;
  SQLCHAR* pwd1;
  SQLCHAR* pwd2;
  SQLCHAR* pwd3;
  BOOL succeed;
};

SQLCHAR * get_connection_string(SQLCHAR   *conn,struct MFA_TEST_DATA *data, BOOL alternative_name)
{
  sprintf((char *)conn, "DSN=%s;DATABASE=test;ENABLE_CLEARTEXT_PLUGIN=1;UID=%s",
                       (char *)mydsn,data->user);
  if(data->pwd)
  {
    strcat((char *)conn, alternative_name ? ";PWD=" : ";PASSWORD=");
    strcat((char *)conn, (const char*)data->pwd);
  }
  if(data->pwd1)
  {
    strcat((char *)conn, alternative_name ? ";PWD1=" : ";PASSWORD1=");
    strcat((char *)conn, (const char*)data->pwd1);
  }
  if(data->pwd2)
  {
    strcat((char *)conn, alternative_name ? ";PWD2=" : ";PASSWORD2=");
    strcat((char *)conn, (const char*)data->pwd2);
  }
  if(data->pwd3)
  {
    strcat((char *)conn, alternative_name ? ";PWD3=" : ";PASSWORD3=");
    strcat((char *)conn, (const char*)data->pwd3);
  }
  if (mydriver != NULL)
  {
    strcat((char *)conn, ";DRIVER=");
    strcat((char *)conn, (char *)mydriver);
  }
  if (mysock != NULL)
  {
    strcat((char *)conn, ";SOCKET=");
    strcat((char *)conn, (char *)mysock);
  }
  if (myport)
  {
    char pbuff[20];
    sprintf(pbuff, ";PORT=%d", myport);
    strcat((char *)conn, pbuff);
  }

  printf("%s\n",conn);

  return conn;
}

DECLARE_TEST(t_mfa_auth)
{
#if MYSQL_VERSION_ID >= 80027
  SQLCHAR   conn[1024], conn_out[1024];
  SQLCHAR   buf[512];
  SQLLEN buflen;
  SQLSMALLINT conn_out_len;
  DECLARE_BASIC_HANDLES(henv1, hdbc1, hstmt1);
  SQLRETURN rc;

  if (!mysql_min_version(hdbc, "8.0.27", 6))
    skip("server does not support mfa");


  struct MFA_TEST_DATA test_data[] =
  {
    //default test
  {myuid, mypwd, NULL   , NULL   , NULL   , TRUE  },
  // user1 tests
  {"user_1f", "pass1", NULL   , NULL   , NULL   , TRUE  },
  {"user_1f", "pass1", "pass1", NULL   , NULL   , TRUE  },
  {"user_1f", "badp1", "pass1", NULL   , NULL   , TRUE  },
  {"user_1f", NULL   , "pass1", NULL   , NULL   , TRUE  },

  {"user_1f", NULL   , NULL   , NULL   , NULL   , FALSE },
  {"user_1f", "badp1", "badp1", "pass1", NULL   , FALSE },
  {"user_1f", NULL   , NULL   , "pass1", NULL   , FALSE },
  {"user_1f", NULL   , NULL   , NULL   , "pass1", FALSE },

  // user2 tests
  {"user_2f", "pass1", NULL   , "pass2", NULL   , TRUE  },
  {"user_2f", "pass1", "pass1", "pass2", NULL   , TRUE  },
  {"user_2f", "badp1", "pass1", "pass2", NULL   , TRUE  },
  {"user_2f", NULL   , "pass1", "pass2", NULL   , TRUE  },

  {"user_2f", "pass2", NULL   , "pass1", NULL   , FALSE },
  {"user_2f", "pass2", "pass2", "pass1", NULL   , FALSE },
  {"user_2f", "pass2", "badp2", "pass1", NULL   , FALSE },
  {"user_2f", NULL   , "pass2", "pass1", NULL   , FALSE },

  {"user_2f", "pass1", NULL   , NULL   , "pass2", FALSE },
  {"user_2f", "pass1", "pass1", NULL   , "pass2", FALSE },
  {"user_2f", "badp1", "pass1", NULL   , "pass2", FALSE },
  {"user_2f", NULL   , "pass1", NULL   , "pass2", FALSE },

  {"user_2f", "pass1", NULL   , "badp1", "pass2", FALSE },
  {"user_2f", "pass1", "pass1", "badp1", "pass2", FALSE },
  {"user_2f", "badp1", "pass1", "badp1", "pass2", FALSE },
  {"user_2f", NULL   , "pass1", "badp1", "pass2", FALSE },

  // user3 tests
  {"user_3f", "pass1", NULL   , "pass2", "pass3", TRUE  },
  {"user_3f", "pass1", "pass1", "pass2", "pass3", TRUE  },
  {"user_3f", "badp1", "pass1", "pass2", "pass3", TRUE  },
  {"user_3f", NULL   , "pass1", "pass2", "pass3", TRUE  },

  {"user_3f", "pass1", NULL   , "pass3", "pass2", FALSE },
  {"user_3f", "pass1", "pass1", "pass3", "pass2", FALSE },
  {"user_3f", "badp1", "pass1", "pass3", "pass2", FALSE },
  {"user_3f", NULL   , "pass1", "pass3", "pass2", FALSE },

  {"user_3f", "pass3", NULL   , "badp1", "pass2", FALSE },
  {"user_3f", "pass3", "pass3", "badp1", "pass2", FALSE },
  {"user_3f", "pass3", "badp3", "badp1", "pass2", FALSE },
  {"user_3f", NULL   , "pass3", "badp1", "pass2", FALSE },

  {"user_3f", "pass1", NULL   , "pass2", "badp3", FALSE },
  {"user_3f", "pass1", "pass1", "pass2", "badp3", FALSE },
  {"user_3f", "badp1", "pass1", "pass2", "badp3", FALSE },
  {"user_3f", NULL   , "pass1", "pass2", "badp3", FALSE },

  };

  SQLExecDirect(hstmt,(SQLCHAR *) "UNINSTALL PLUGIN cleartext_plugin_server",SQL_NTS);

  rc = SQLExecDirect(hstmt,(SQLCHAR *) "INSTALL PLUGIN cleartext_plugin_server SONAME 'auth_test_plugin.so'",SQL_NTS);

  if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
  {
    SKIP_REASON = "Server doesn't support cleartext_plugin_server";
    return SKIP;
  }

  ok_sql(hstmt, "drop user if exists  user_1f");
  ok_sql(hstmt, "drop user if exists  user_2f");
  ok_sql(hstmt, "drop user if exists  user_3f");

  ok_sql(hstmt, "create user user_1f IDENTIFIED WITH cleartext_plugin_server BY 'pass1'");
  ok_sql(hstmt, "grant all on *.* to user_1f");
  ok_sql(hstmt, "create user user_2f IDENTIFIED WITH cleartext_plugin_server BY 'pass1' "\
                "AND IDENTIFIED WITH cleartext_plugin_server BY 'pass2'; ");
  ok_sql(hstmt, "grant all on *.* to user_2f");
  ok_sql(hstmt, "create user user_3f IDENTIFIED WITH cleartext_plugin_server by 'pass1' "\
                "AND IDENTIFIED WITH cleartext_plugin_server BY 'pass2' "\
                "AND IDENTIFIED WITH cleartext_plugin_server BY 'pass3'; ");
  ok_sql(hstmt, "grant all on *.* to user_3f");


  if(myenable_pooling)
  {
    //Enable pooling
    rc = SQLSetEnvAttr(NULL, SQL_ATTR_CONNECTION_POOLING,
                       (SQLPOINTER)SQL_CP_ONE_PER_HENV, 0);
    if(!SQL_SUCCEEDED(rc))
      TEST_RETURN_FAIL;
  }


  ok_env(henv1, SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv1));

#ifdef SQL_OV_ODBC3_80
  if(myenable_pooling)
  {
    ok_env(henv1, SQLSetEnvAttr(henv1, SQL_ATTR_ODBC_VERSION,
                                (SQLPOINTER)SQL_OV_ODBC3_80, 0));
  }
  else
#endif
  {
    ok_env(henv1, SQLSetEnvAttr(henv1, SQL_ATTR_ODBC_VERSION,
                                (SQLPOINTER)SQL_OV_ODBC3, 0));

  }

  if(myenable_pooling)
  {

    ok_env(henv1,SQLSetEnvAttr(henv1, SQL_ATTR_CP_MATCH, (SQLPOINTER)SQL_CP_RELAXED_MATCH, 0));
  }


  for(int i=0; i < sizeof(test_data)/sizeof(struct MFA_TEST_DATA); ++i)
  {
    ok_env(henv1, SQLAllocHandle(SQL_HANDLE_DBC, henv1, &hdbc1));

    struct MFA_TEST_DATA *data = &test_data[i];

    printf("User: %s\n", data->user);
    printf("PWD: %s\n", data->pwd ? data->pwd : "-");
    printf("PWD1: %s\n", data->pwd1 ? data->pwd1 : "-");
    printf("PWD2: %s\n", data->pwd2 ? data->pwd2 : "-");
    printf("PWD3: %s\n", data->pwd3 ? data->pwd3 : "-");

    BOOL use_alternative_parameter_name = rand()%2;

    if(data->succeed)
    {
      rc = SQLDriverConnect(
            hdbc1, NULL,
            get_connection_string(conn, data, use_alternative_parameter_name),
            SQL_NTS, conn_out,
            sizeof(conn_out), &conn_out_len,
            SQL_DRIVER_NOPROMPT);
      if(!SQL_IS_SUCCESS(rc))
        TEST_RETURN_FAIL;

      printf("CONN OUT: %s\n", conn_out);

    }
    else
    {
      rc = SQLDriverConnect(
        hdbc1, NULL,
        get_connection_string(conn, data, use_alternative_parameter_name),
        SQL_NTS, conn_out,
        sizeof(conn_out), &conn_out_len,
        SQL_DRIVER_NOPROMPT);
      if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO)
      {
        TEST_RETURN_FAIL;
      }

      ok_con(hdbc1, SQLFreeConnect(hdbc1));

      //we will continue, since no connection is available
      continue;
    }

    ok_con(hdbc1, SQLAllocStmt(hdbc1, &hstmt1));

    ok_sql(hstmt1, "select @@version");
    ok_stmt(hstmt1,SQLFetch(hstmt1));

    SQLGetData(hstmt1, 1, SQL_CHAR, buf, sizeof(buf), &buflen);
    printf("Server version: %s\n",buf);

    ok_stmt(hstmt1, SQLFreeStmt(hstmt1, SQL_CLOSE));

    ok_con(hdbc1, SQLDisconnect(hdbc1));
    ok_con(hdbc1, SQLFreeConnect(hdbc1));

    ok_env(henv1, SQLAllocHandle(SQL_HANDLE_DBC, henv1, &hdbc1));

    rc = SQLDriverConnect(
          hdbc1, NULL,
          get_connection_string(conn, data, use_alternative_parameter_name),
          SQL_NTS, conn_out,
          sizeof(conn_out), &conn_out_len,
          SQL_DRIVER_NOPROMPT);

    if(!SQL_IS_SUCCESS(rc))
      TEST_RETURN_FAIL;

    printf("CONN OUT: %s\n", conn_out);

    ok_con(hdbc1, SQLAllocStmt(hdbc1, &hstmt1));

    ok_sql(hstmt1, "select @@version");
    ok_stmt(hstmt1,SQLFetch(hstmt1));

    SQLGetData(hstmt1, 1, SQL_CHAR, buf, sizeof(buf), &buflen);
    printf("Server version: %s\n",buf);

    ok_stmt(hstmt1, SQLFreeStmt(hstmt1, SQL_CLOSE));

    ok_con(hdbc1, SQLDisconnect(hdbc1));
    ok_con(hdbc1, SQLFreeConnect(hdbc1));

  }


  ok_sql(hstmt, "drop user if exists  user_1f");
  ok_sql(hstmt, "drop user if exists  user_2f");
  ok_sql(hstmt, "drop user if exists  user_3f");

  ok_env(henv1, SQLFreeEnv(henv1));

#endif

  return OK;
}



DECLARE_TEST(t_dummy_test)
{
  return OK;
}


BEGIN_TESTS
  // ADD_TEST(t_plugin_auth) TODO: Fix
  ADD_TEST(t_dummy_test)
  ADD_TEST(t_ldap_auth)
#if MFA_ENABLED
  ADD_TEST(t_mfa_auth)
#endif
  END_TESTS

RUN_TESTS
