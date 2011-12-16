// This file is based on BansheeDbConnection.cs from the Banshee source code
// #include <qstring.h>

/*
using System;
using System.Linq;
using System.Collections.Generic;
using System.IO;
using System.Threading;

using Hyena;
using Hyena.Data;
using Hyena.Jobs;
using Hyena.Data.Sqlite;

using Banshee.Base;
using Banshee.ServiceStack;
using Banshee.Configuration;
*/

class BansheeDbConnection //  HyenaSqliteConnection, IInitializeService, IRequiredService
{
public:
    BansheeDbConnection();
    virtual ~BansheeDbConnection();

    static QString getDatabaseFile();

    /*
  public:
    BansheeDbConnection() : this (DatabaseFile)
    {
   }

  private:
    BansheeDbFormatMigrator migrator;
    DatabaseConfigurationClient configuration;
*/
};
