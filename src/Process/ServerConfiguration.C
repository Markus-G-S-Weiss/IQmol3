/*******************************************************************************

  Copyright (C) 2022 Andrew Gilbert

  This file is part of IQmol, a free molecular visualization program. See
  <http://iqmol.org> for more details.

  IQmol is free software: you can redistribute it and/or modify it under the
  terms of the GNU General Public License as published by the Free Software  
  Foundation, either version 3 of the License, or (at your option) any later  
  version.

  IQmol is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along
  with IQmol.  If not, see <http://www.gnu.org/licenses/>.
   
********************************************************************************/

#include "ServerConfiguration.h"
#include "QsLog.h"
#include "SystemDependent.h"
#include "ParseFile.h"
#include <QDir>


namespace IQmol {
namespace Process {

void ServerConfiguration::dump() const 
{
   qDebug() << " ==> Server Configuration <== ";
   for (int i = 0; i < MaxFieldT; ++i) {
       FieldT field(static_cast<FieldT>(i));
       QString name(toString(field));
       qDebug() << "    " << name << "=>" << value(field);
   }
}

QString ServerConfiguration::toString(FieldT const field)
{
   QString s;
   switch (field) {
      case ServerName:          s = "Server Name";          break;
      case Connection:          s = "Connection";           break;
      case QueueSystem:         s = "Queue System";         break;
      case HostAddress:         s = "Host Address";         break;
      case UserName:            s = "User Name";            break;
      case Port:                s = "Port";                 break;
      case Authentication:      s = "Authentication";       break;
      case WorkingDirectory:    s = "Working Directory";    break;
      case Submit:              s = "Submit";               break;
      case Query:               s = "Query";                break;
      case QueueInfo:           s = "Queue Info";           break;
      case Kill:                s = "Kill";                 break;
      case UpdateInterval:      s = "Update Interval";      break;
      case JobLimit:            s = "Job Limit";            break;
      case RunFileTemplate:     s = "Run File Template";    break;
      case Cookie:              s = "Cookie";               break;
      case QueueResources:      s = "Queue Resources";      break;
      case JobFileList:         s = "Job File List";        break;
      case PrivateKeyFile:      s = "SSH Private Key File"; break;
      case PublicKeyFile:       s = "SSH Public Key File";  break;
      case KnownHostsFile:      s = "SSH Known Hosts File"; break;
      case AuthenticationPort:  s = "Authentication Port";  break;

      case AwsRegion:           s = "AWS Region";           break;
      case CognitoUserPool:     s = "Cognito User Pool";    break;
      case CognitoAppClient:    s = "Cognito App Client";   break;
      case ApiGateway:          s = "Api Gateway";          break;

      case MaxFieldT:           s = "";                     break;
   }
   return s;
}


QString ServerConfiguration::toString(QueueSystemT const queue)
{
   QString s;
   switch (queue) {
      case Basic:   s = "Basic";  break;
      case PBS:     s = "PBS";    break;
      case SGE:     s = "SGE";    break;
      case SLURM:   s = "SLURM";  break;
      case Web:     s = "Web";    break;
      case AWS:     s = "AWS";    break;
   }
   return s;
}


ServerConfiguration::FieldT ServerConfiguration::toFieldT(QString const& field)
{ 
   if (field.contains("server",              Qt::CaseInsensitive))  return ServerName;
   if (field.contains("connection",          Qt::CaseInsensitive))  return Connection;
   if (field.contains("info",                Qt::CaseInsensitive))  return QueueInfo;
   if (field.contains("queue",               Qt::CaseInsensitive))  return QueueSystem;
   if (field.contains("host",                Qt::CaseInsensitive))  return HostAddress;
   if (field.contains("address",             Qt::CaseInsensitive))  return HostAddress;
   if (field.contains("user",                Qt::CaseInsensitive))  return UserName;
   if (field.contains("authentication port", Qt::CaseInsensitive))  return AuthenticationPort;
   if (field.contains("port",                Qt::CaseInsensitive))  return Port;
   if (field.contains("auth",                Qt::CaseInsensitive))  return Authentication;
   if (field.contains("dir",                 Qt::CaseInsensitive))  return WorkingDirectory;
   if (field.contains("submit",              Qt::CaseInsensitive))  return Submit;
   if (field.contains("query",               Qt::CaseInsensitive))  return Query;
   if (field.contains("kill",                Qt::CaseInsensitive))  return Kill;
   if (field.contains("interval",            Qt::CaseInsensitive))  return UpdateInterval;
   if (field.contains("limit",               Qt::CaseInsensitive))  return JobLimit;
   if (field.contains("template",            Qt::CaseInsensitive))  return RunFileTemplate;
   if (field.contains("resources",           Qt::CaseInsensitive))  return QueueResources;
   if (field.contains("list",                Qt::CaseInsensitive))  return JobFileList;
   if (field.contains("cookie",              Qt::CaseInsensitive))  return Cookie;
   if (field.contains("publickey",           Qt::CaseInsensitive))  return PublicKeyFile;
   if (field.contains("privatekey",          Qt::CaseInsensitive))  return PrivateKeyFile;
   if (field.contains("knownhosts",          Qt::CaseInsensitive))  return KnownHostsFile;

   return MaxFieldT;
}


ServerConfiguration::QueueSystemT ServerConfiguration::toQueueSystemT(
   QString const& queueSystem)
{
   if (queueSystem.contains("pbs",   Qt::CaseInsensitive)) return PBS;
   if (queueSystem.contains("sge",   Qt::CaseInsensitive)) return SGE;
   if (queueSystem.contains("web",   Qt::CaseInsensitive)) return Web;
   if (queueSystem.contains("aws",   Qt::CaseInsensitive)) return AWS;
   if (queueSystem.contains("slurm", Qt::CaseInsensitive)) return SLURM;

   return Basic;
}


ServerConfiguration::ServerConfiguration()
{
#ifdef Q_OS_WIN32
   // Set the default to HTTP on Windows for now as HTTPS seems to require
   // additional libraries.
   setDefaults(Network::HTTP);
#else
   setDefaults(Network::HTTP);
#endif
   setDefaults(Web);
   //setDefaults(Local);
   //setDefaults(Basic);
}


ServerConfiguration::ServerConfiguration(ServerConfiguration const& that)
{
   copy(that); 
}


ServerConfiguration& ServerConfiguration::operator=(ServerConfiguration const& that)
{
   if (this != &that) copy(that);
   return *this;
}


void ServerConfiguration::copy(ServerConfiguration const& that)
{
   // Al the data members are held in the ConfigMap
   m_configuration = that.m_configuration;
}


ServerConfiguration::ServerConfiguration(QVariant const& qvar)
{
   fromQVariant(qvar);
}


ServerConfiguration::ServerConfiguration(YAML::Node const& node)
{
   fromYamlNode(node);
}


QString ServerConfiguration::value(FieldT const field) const
{
   if (!m_configuration.contains(field) && field != Cookie) {
      QLOG_WARN() << "Server configuration field not found" << toString(field);
   }

   QString s;

   switch (field) {
      
      case Authentication:
         s = Network::ToString(authentication());
         break;

      case QueueSystem:
         s = toString(queueSystem());
         break;

      case Connection:
         s = Network::ToString(connection());
         break;

      case Port:
         s = QString::number(port());
         break;

      case AuthenticationPort:
         s = QString::number(authenticationPort());
         break;

      case UpdateInterval:
         s = QString::number(updateInterval());
         break;

      default:
         s = m_configuration.value(field).toString();
         break;
   }

   return s;
}


void ServerConfiguration::setValue(FieldT const field, QVariant const& value)
{
   m_configuration.insert(field, value);
}


Network::ConnectionT ServerConfiguration::connection() const
{
   return static_cast<Network::ConnectionT>(m_configuration.value(Connection).toInt());
}


ServerConfiguration::QueueSystemT ServerConfiguration::queueSystem() const
{
   return static_cast<QueueSystemT>(m_configuration.value(QueueSystem).toInt());
}


Network::AuthenticationT ServerConfiguration::authentication() const
{
   return static_cast<Network::AuthenticationT>(m_configuration.value(Authentication).toInt());
}


int ServerConfiguration::authenticationPort() const
{
   return m_configuration.value(AuthenticationPort).toInt();
}


int ServerConfiguration::port() const
{
   return m_configuration.value(Port).toInt();
}


int ServerConfiguration::updateInterval() const
{
   return m_configuration.value(UpdateInterval).toInt();
}

QVariantList ServerConfiguration::queueResourcesList() const
{
   return m_configuration.value(QueueResources).toList();
}


void ServerConfiguration::setDefaults(Network::ConnectionT const connection)
{
   qDebug() << "Setting defaults for: " << Network::ToString(connection);
   m_configuration.insert(Connection, (int)connection);

   switch (connection) {

      case Network::Local:
         m_configuration.insert(ServerName, "Local");
         m_configuration.insert(Port, 0);
         m_configuration.insert(HostAddress, "localhost");
         m_configuration.insert(Authentication, Network::Anonymous);
         m_configuration.insert(UserName, QString(qgetenv("USER")));
         m_configuration.insert(WorkingDirectory, QDir::homePath());
         m_configuration.insert(WorkingDirectory, "(unused)");
         break;

      case Network::SSH:
         m_configuration.insert(ServerName, "Server");
         m_configuration.insert(Port, 22);
         m_configuration.insert(Authentication, Network::Password);
         m_configuration.insert(UserName, QString(qgetenv("USER")));
         m_configuration.insert(WorkingDirectory, "");
         break;

      case Network::SFTP:
         m_configuration.insert(ServerName, "Server");
         m_configuration.insert(Port, 22);
         m_configuration.insert(Authentication, Network::Password);
         m_configuration.insert(UserName, QString(qgetenv("USER")));
         m_configuration.insert(WorkingDirectory, "");
         break;

      case Network::HTTP:
         m_configuration.insert(ServerName, "Q-Chem");
         m_configuration.insert(Port, 80);
         m_configuration.insert(HostAddress, "iqmol.q-chem.com");
         m_configuration.insert(UserName, "guest");
         m_configuration.insert(WorkingDirectory, "(unused)");
         m_configuration.insert(Authentication, Network::Anonymous);
         break;

      case Network::HTTPS:
         m_configuration.insert(ServerName, "Q-Chem");
         m_configuration.insert(Port, 443);
         m_configuration.insert(HostAddress, "iqmol.q-chem.com");
         m_configuration.insert(UserName, "guest");
         m_configuration.insert(WorkingDirectory, "(unused)");
         m_configuration.insert(Authentication, Network::Anonymous);
         break;

      case Network::AWS:
         m_configuration.insert(ServerName, "Q-Cloud");
         m_configuration.insert(Port, 443);
         m_configuration.insert(HostAddress, "API Gateway");
         m_configuration.insert(UserName, "");
         m_configuration.insert(WorkingDirectory, "(unused)");
         m_configuration.insert(Authentication, Network::Password);
         break;
   }
}


void ServerConfiguration::setDefaults(QueueSystemT const queueSystem)
{
   qDebug() << "Setting defaults for: " << toString(queueSystem);
   m_configuration.insert(UpdateInterval, 20);
   m_configuration.insert(QueueSystem, queueSystem);

   switch (queueSystem) {

      case Basic: {
         bool local(connection() == Network::Local);
         if (local) m_configuration.insert(UpdateInterval, 10);
         m_configuration.insert(Submit, System::SubmitCommand(local));
         m_configuration.insert(Query,  System::QueryCommand(local));
         m_configuration.insert(Kill,   System::KillCommand(local));
         m_configuration.insert(QueueInfo,   "qcprog.exe");
         m_configuration.insert(RunFileTemplate, System::TemplateForRunFile(local));
         m_configuration.insert(JobLimit, 1024);
         m_configuration.insert(JobFileList, System::JobFileListCommand(local));
      } break;

      case PBS: {
         m_configuration.insert(Kill, "qdel ${JOB_ID}");
         m_configuration.insert(Query, "qstat -xf ${JOB_ID}");
         m_configuration.insert(Submit, "cd ${JOB_DIR} && qsub ${JOB_NAME}.run");
         m_configuration.insert(QueueInfo, "qstat -fQ");
         m_configuration.insert(JobFileList, "find ${JOB_DIR} -type f");
    
         QStringList runFile;
         runFile << "#!/bin/csh"
                 << "#PBS -q ${QUEUE}"
                 << "#PBS -l walltime=${WALLTIME}"
                 << "#PBS -l mem=${MEMORY}Mb"
                 << "#PBS -l jobfs=${SCRATCH}Mb"
                 << "#PBS -l ncpus=${NCPUS}"
                 << "#PBS -j oe"
                 << "#PBS -o ${JOB_NAME}.err"
                 << "#PBS -l wd"
                 << ""
                 << "setenv QC /usr/local/qchem"
                 << "setenv QCAUX $QC/aux"
                 << "setenv QCSCRATCH $PBS_JOBFS"
                 << "if (-e $QC/bin/qchem.setup) source $QC/bin/qchem.setup"
                 << ""
                 << "qchem -nt ${NCPUS} ${JOB_NAME}.inp ${JOB_NAME}.out";
    
         m_configuration.insert(RunFileTemplate, runFile.join("\n"));

      } break;

      case SGE: {
         m_configuration.insert(Kill, "qdel ${JOB_ID}");
         // This is annoying, but SGE qstat -j doesn't give us the status.
         m_configuration.insert(Query, "qstat && qstat -j ${JOB_ID}");
         m_configuration.insert(Submit, "cd ${JOB_DIR} && qsub ${JOB_NAME}.run");
         m_configuration.insert(QueueInfo, "qstat -g c");
         m_configuration.insert(JobFileList, "find ${JOB_DIR} -type f");
   
         QStringList runFile;
         runFile << "#!/bin/csh"
                 << "#$ -S /bin/csh"
                 << "#$ -q ${QUEUE}"
                 << "#$ -l h_rt=${WALLTIME}"
                 << "#$ -l h_vmem=${MEMORY}"
                 << "#$ -l scr_free=${SCRATCH}"
                 << "#$ -pe mpi 1"
                 << "#$ -j yes"            //merge stderr stdout
                 << "#$ -cwd"     
                 << ""
                 << "setenv QC /usr/local/qchem"
                 << "setenv QCAUX $QC/aux"
                 << "setenv QCSCRATCH $TMPDIR"
                 << "if (-e $QC/bin/qchem.setup) source $QC/bin/qchem.setup"
                 << ""
                 << "qchem -nt ${NCPUS} ${JOB_NAME}.inp ${JOB_NAME}.out";

         m_configuration.insert(RunFileTemplate, runFile.join("\n"));

      } break;

      case SLURM: {
         m_configuration.insert(Submit, "cd ${JOB_DIR} && sbatch ${JOB_NAME}.run");
         //m_configuration.insert(Query, "squeue -j ${JOB_ID} -o %20T");
         // ? m_configuration.insert(Query, "scontrol show job ${JOB_ID}");
         m_configuration.insert(Query, "sacct -X -n -ostate -j ${JOB_ID}");
         m_configuration.insert(Kill, "scancel ${JOB_ID}");
         m_configuration.insert(JobFileList, "find ${JOB_DIR} -type f");

         m_configuration.insert(QueueInfo, "sinfo");

   
         QStringList runFile;
         runFile << "#!/bin/csh"
                 << "#SBATCH --partition=${QUEUE}"
                 << "#SBATCH --time=${WALLTIME}"
                 << "#SBATCH --mem=${MEMORY}M"
                 << "#SBATCH --tmp=${SCRATCH}M"
                 << "#SBATCH --nodes=1"
                 << "#SBATCH --cpus-per-task=${NCPUS}"
                 << "#SBATCH --output=${JOB_NAME}.err"
                 << "#SBATCH --error=${JOB_NAME}.err"
                 << ""
                 << "setenv QCSCRATCH $TMPDIR"
                 << "setenv QC /usr/local/qchem"
                 << "setenv QCAUX $QC/aux"
                 << ""
                 << "if (-e $QC/bin/qchem.setup) source $QC/bin/qchem.setup"
                 << ""
                 << "qchem -nt ${NCPUS} ${JOB_NAME}.inp ${JOB_NAME}.out";
 
         m_configuration.insert(RunFileTemplate, runFile.join("\n"));

      } break;

      case Web: {
         m_configuration.insert(Kill,      "GET  /delete?cookie=${COOKIE}&jobid=${JOB_ID}");
         m_configuration.insert(Query,     "GET  /status?cookie=${COOKIE}&jobid=${JOB_ID}");
         m_configuration.insert(Submit,    "POST /submit?cookie=${COOKIE}");
         m_configuration.insert(QueueInfo, 
            "GET  /download?cookie=${COOKIE}&jobid=${JOB_ID}&file=${FILE_NAME}");
         m_configuration.insert(RunFileTemplate, "(unused)");
         m_configuration.insert(JobFileList, "GET /list?cookie=${COOKIE}&jobid=${JOB_ID}");
      } break;

      case AWS: {
         m_configuration.insert(Submit,      "POST /exec/qcloud?function=submit");
         m_configuration.insert(Kill,        "GET  /exec/qcloud?function=cancel&job_id=${JOB_ID}");
         m_configuration.insert(Query,       "GET  /exec/qcloud?function=status&job_id=${JOB_ID}");
         m_configuration.insert(QueueInfo,   "GET  /exec/qcloud?function=download&job_id=${JOB_ID}");
         m_configuration.insert(JobFileList, "GET  /exec/qcloud?function=meta&job_id=${JOB_ID}"); // file size
         m_configuration.insert(RunFileTemplate, "(unused)");
      } break;
   }
}


// This is required as a ConfigMap doesn't convert to a QVariant, whereas
// a QVariantMap does (used in the preferences).
QVariant ServerConfiguration::toQVariant() const
{
   QVariantMap map;

   ConfigMap::const_iterator iter;
   for (iter = m_configuration.begin(); iter != m_configuration.end(); ++iter) {
       map.insert(toString(iter.key()), iter.value());
   }

   return QVariant(map);
}


void ServerConfiguration::fromQVariant(QVariant const& qvar)
{
   QVariantMap map(qvar.toMap());
   if (map.contains("Type")) {
      // Assume we are dealing with an old server configuration
      fromQVariantMapLegacy(map);
   }else {
      fromQVariantMap(map);
   }
}


void ServerConfiguration::fromQVariantMap(QVariantMap const& map) 
{
   for (int i = 0; i < MaxFieldT; ++i) {
       FieldT field(static_cast<FieldT>(i));
       QString name(toString(field));
       if (map.contains(name)) {
          m_configuration.insert(field, map.value(name));
       }
   }
}


Data::YamlNode ServerConfiguration::toYamlNode() const
{
   Data::YamlNode node;
qDebug() << "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv";
qDebug() << "Exporting configuration:";
dump();
qDebug() << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^";


   ConfigMap::const_iterator iter;
   for (iter = m_configuration.begin(); iter != m_configuration.end(); ++iter) {
       std::string key(toString(iter.key()).toStdString());
       std::string value;

       switch (iter.key()) {
          case Authentication:
             value = Network::ToString(authentication()).toStdString();
             break;
          case QueueSystem:
             value = toString(queueSystem()).toStdString();
             break;
          case Connection:
             value = Network::ToString(connection()).toStdString();
             break;
          default:
             value = iter.value().toString().toStdString();
             break;
       }

       node[key] = value;
   }

   return node;
}


void ServerConfiguration::fromYamlNode(YAML::Node const& node)
{
   if (node.Type() != YAML::NodeType::Map) return;

   YAML::const_iterator iter;
   for (iter = node.begin(); iter != node.end(); ++iter) {

       std::string s(iter->first.as<std::string>());
       FieldT field(toFieldT(QString::fromStdString(s)));
       s = iter->second.as<std::string>();

       switch (field) {
          case Connection:
             m_configuration.insert(field, (int)Network::ToConnectionT(QString::fromStdString(s)));
             break;
          case Authentication:
             m_configuration.insert(field, (int)Network::ToAuthenticationT(QString::fromStdString(s)));
             break;
          case QueueSystem:
             m_configuration.insert(field, (int)toQueueSystemT(QString::fromStdString(s)));
             break;
          default:
             m_configuration.insert(field, QString::fromStdString(s));
             break;
       }
   }
}


void ServerConfiguration::fromQVariantMapLegacy(QVariantMap const& map) 
{
  //toLower();
   bool ok;
      
   if (map.contains("Name") && !map.value("Name").toString().isEmpty()) {
      m_configuration.insert(ServerName, map.value("Name"));
   } else {
      m_configuration.insert(ServerName, "Server");
      QLOG_WARN() << "Server name not set in ServerConfiguration::fromQVariantMapLegacy";
   }        
            
   if (map.contains("Host")) {
      // old enum Host { Local = 0, Remote, Web };          
      int host(map.value("Host").toInt(&ok));
      if (ok) {
         Network::ConnectionT connection(Network::Local);
         switch (host) {
            case 0:  connection = Network::Local;  break;
            case 1:  connection = Network::SSH;    break;
            case 2:  connection = Network::HTTP;   break;
         }
         setDefaults(connection);    
      }
   }
   
   if (map.contains("Type")) {
      // old enum Type { Basic = 0, PBS, SGE, HTTP};
      int type(map.value("Type").toInt(&ok));
      if (ok) {
         QueueSystemT queue(Basic);
         switch (type) {
            case 0:  queue = Basic;  break;
            case 1:  queue = PBS;    break;
            case 2:  queue = SGE;    break;
            case 3:  queue = Web;    break;  // Shouldn't have been used
         }
         setDefaults(queue);
      }
   }  
      
   if (map.contains("Authentication")) {
      // old  Authentication { None = 0, Agent, PublicKey, HostBased, KeyboardInteractive, 
      //                       Vault, Prompt   };
      int a(map.value("Authentication").toInt(&ok));
      if (ok) { 
         Network::AuthenticationT auth(Network::Password);

         switch (a) { 
            // Note None, and Vault are no longer supported, so we default to Password
            case 0:  auth = Network::Password;             break; 
            case 1:  auth = Network::Agent;                break;
            case 2:  auth = Network::PublicKey;            break;
            case 3:  auth = Network::HostBased;            break;
            case 4:  auth = Network::KeyboardInteractive;  break;
            case 5:  auth = Network::Password;             break;
            case 6:  auth = Network::Password;             break;
         }
         m_configuration.insert(Authentication, auth);
      }                
   }                   
                       
   m_configuration.insert(JobLimit,         map.value("JobLimit"));
   m_configuration.insert(HostAddress,      map.value("HostAddress"));
   m_configuration.insert(UserName,         map.value("UserName"));
   m_configuration.insert(Port,             map.value("Port"));
   m_configuration.insert(WorkingDirectory, map.value("WorkingDirectory"));
   m_configuration.insert(UpdateInterval,   map.value("UpdateInterval"));
         
   m_configuration.insert(Submit,           map.value("SubmitCommand"));
   m_configuration.insert(Query,            map.value("QueryCommand"));
   m_configuration.insert(QueueInfo,        map.value("QueueInfo"));
   m_configuration.insert(Kill,             map.value("KillCommand"));
         
   m_configuration.insert(RunFileTemplate,  map.value("RunFileTemplate"));
         
   //if (map.contains("DelegateDefaults")) {
   //   m_delegateDefaults = map.value("DelegateDefaults").toMap();
   //}
}


bool ServerConfiguration::loadFromFile(QString const& filePath)
{
   bool ok(false);

   QFile file(filePath);
   if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QLOG_ERROR() << "Server configuraiton file does not exist" << filePath;
      return ok;
   }   

   Parser::ParseFile parser(filePath);
   parser.start();
   parser.wait();

   QStringList errors(parser.errors());
   if (!errors.isEmpty()) {
      QLOG_ERROR() << errors.join("\n");
   }   

   Data::Bank& bank(parser.data());
   QList<Data::YamlNode*> yaml(bank.findData<Data::YamlNode>());
   if (yaml.first()) {
      yaml.first()->dump();
      *this = ServerConfiguration(*(yaml.first()));
      ok = true;
   }   

   return ok;
}

} } // end namespace IQmol::Process
