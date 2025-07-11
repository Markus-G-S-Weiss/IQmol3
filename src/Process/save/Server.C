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

#include "QtVersionHacks.h"
#include "Server.h"
#include "ServerRegistry.h"
#include "Reply.h"
#include "Connection.h"
#include "LocalConnection.h"
#include "SshConnection.h"
#include "HttpConnection.h"
#include "AwsConnection.h"
#include "WriteToTemporaryFile.h"
#include "SystemDependent.h"
#include "TextStream.h"
#include "JobMonitor.h"
#include "Preferences.h"
#include "QsLog.h"
#include <QDebug>
#include <QRegularExpression>


namespace IQmol {
namespace Process {

Server::Server(ServerConfiguration const& configuration) : m_configuration(configuration),
   m_connection(0)
{
   //qDebug() << "Constructing a New Server with configuration";
   //m_configuration.dump();
   setUpdateInterval(m_configuration.updateInterval());
   connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(queryAllJobs()));
}


Server::~Server()
{
   closeConnection();
}


QString Server::name() const
{
   return m_configuration.value(ServerConfiguration::ServerName);
}


QStringList Server::tableFields() const
{
   QStringList fields;
   fields.append(m_configuration.value(ServerConfiguration::ServerName));
   fields.append(m_configuration.value(ServerConfiguration::HostAddress));
   fields.append(m_configuration.value(ServerConfiguration::Connection));
   fields.append(m_configuration.value(ServerConfiguration::UserName));
   return fields;
}


void Server::closeConnection()
{
   if (m_connection) {
      m_connection->close();
      delete m_connection;
      m_connection = 0;
   }
}


bool Server::open()
{
   bool ok(true);
   // Short circuit the open if we have already been authenticated.
   if (m_connection && m_connection->status() == Network::Authenticated) return true;
   stopUpdates();

   QString address(m_configuration.value(ServerConfiguration::HostAddress));
   QString publicKeyFile(m_configuration.value(ServerConfiguration::PublicKeyFile));
   QString privateKeyFile(m_configuration.value(ServerConfiguration::PrivateKeyFile));
   QString knownHostsFile(m_configuration.value(ServerConfiguration::KnownHostsFile));
   
   Network::AwsConfig awsConfig;
   awsConfig.AwsRegion        = m_configuration.value(ServerConfiguration::AwsRegion);
   awsConfig.CognitoUserPool  = m_configuration.value(ServerConfiguration::CognitoUserPool);
   awsConfig.CognitoAppClient = m_configuration.value(ServerConfiguration::CognitoAppClient);
   awsConfig.ApiGateway       = m_configuration.value(ServerConfiguration::ApiGateway);

   if (!m_connection) {
      QLOG_TRACE() << "Creating connection" 
                   << m_configuration.value(ServerConfiguration::Connection);

      int port(m_configuration.port());

      switch (m_configuration.connection()) {
         case Network::Local:
            m_connection = new Network::LocalConnection();
            break;
         case Network::SSH:
         case Network::SFTP:
            m_connection = new Network::SshConnection(address, port, 
               publicKeyFile, privateKeyFile, knownHostsFile, true);
            break;
         case Network::HTTP:
            m_connection = new Network::HttpConnection(address, port);
            break;
         case Network::HTTPS:
            m_connection = new Network::HttpConnection(address, port, true);
            break;
         case Network::AWS:
            m_connection = new Network::AwsConnection(awsConfig, port);
            break;
      }
   }

   if (m_connection->status() == Network::Closed) {
      m_connection->open();
   }

   if (m_connection->status() == Network::Opened) {
      QLOG_TRACE() << "Authenticating connection";
      Network::AuthenticationT 
         authentication(m_configuration.authentication());

      if (m_configuration.isWebBased() && m_configuration.connection() != Network::AWS) {
         QString cookie(m_configuration.value(ServerConfiguration::Cookie));

         if (authentication == Network::Password) {
            // Update our JWT through the authentication server.
            int authenticationPort(m_configuration.authenticationPort());
            QString userName(m_configuration.value(ServerConfiguration::UserName));
            QString jwt(userName);

            // Spawn a separate connection as the authentication server may
            // be on a different machine.
            Network::HttpConnection conn(address, authenticationPort);
            conn.open();
            conn.authenticate(authentication, jwt);
            if (conn.status() == Network::Authenticated) {
               cookie = jwt;
               m_connection->setStatus(Network::Authenticated);
            }else {
               m_connection->setMessage(conn.message());
            }
         }else {
            m_connection->authenticate(authentication, cookie);
         }

         m_configuration.setValue(ServerConfiguration::Cookie, cookie);
         ServerRegistry::save();

      }else {
         QString userName(m_configuration.value(ServerConfiguration::UserName));
         m_connection->authenticate(authentication, userName);
      }
   }

   if (m_connection->status() == Network::Authenticated) {
      if (!m_watchedJobs.isEmpty()) {
         queryAllJobs();
         startUpdates();
      }
   }else {
      m_message = m_connection->message();
      qDebug() << "Setting Server message" << m_message;
      delete m_connection;
      m_connection = 0;
      ok = false;
   }

   return ok;
}


bool Server::exists(QString const& filePath)
{
   return open() && m_connection->exists(filePath);
}


QString Server::queueInfo()
{
   QString info;
   if (open() ) {
      QString cmd(m_configuration.value(ServerConfiguration::QueueInfo));
      cmd = substituteMacros(cmd);
      m_connection->blockingExecute(cmd, &info);
   }
   return info;
}


bool Server::removeDirectory(QString const& path)
{
   return open() && m_connection->removeDirectory(path);
}


bool Server::makeDirectory(QString const& path)
{
   return open() && m_connection->makeDirectory(path);
}


void Server::queryAllJobs()
{
   qDebug() << "QueryAllJobs called()";
   // This is a bit of a hack, update this here in case the user
   // has modified the server while running.
   setUpdateInterval(m_configuration.updateInterval());

   if (m_watchedJobs.isEmpty() || m_connection->status() != Network::Authenticated) {
      stopUpdates(); 
      return;
   }

   QList<Job*>::iterator iter;
   for (iter = m_watchedJobs.begin(); iter != m_watchedJobs.end(); ++iter) {
       query(*iter);
   }
}


// ---------- Submit ----------
void Server::submit(Job* job)
{
   QList<Network::Reply*> keys(m_activeRequests.keys(job));

   if (!job) throw Exception("Submit called with null job");
   if (m_watchedJobs.contains(job)) throw Exception("Attempt to submit duplicate job");
   if (!keys.isEmpty()) throw Exception("Attempt to submit busy job");

   if (!open()) return;

   QString contents(job->jobInfo().get(QChemJobInfo::InputString));
   QString fileName(Util::WriteToTemporaryFile(contents));
   QLOG_DEBUG() << "Input file contents written to" << fileName;

   if (isLocal()) job->jobInfo().localFilesExist(true);

   // In the case of an HTTP server, we can simply POST the contents of the
   // input file and we're done.  Other servers need the run file and a 
   // separate submission step.
   if (isWebBased()) {
      QString submit(m_configuration.value(ServerConfiguration::Submit));
      submit = substituteMacros(submit);
      Network::Reply* reply(m_connection->putFile(fileName, submit));
      connect(reply, SIGNAL(finished()), this, SLOT(submitFinished()));
      m_activeRequests.insert(reply, job);
      reply->start();
   }else {
      QString destination(job->jobInfo().getRemoteFilePath(QChemJobInfo::InputFileName));
      Network::Reply* reply(m_connection->putFile(fileName, destination));
      connect(reply, SIGNAL(finished()), this, SLOT(copyRunFile()));
      m_activeRequests.insert(reply, job);
      reply->start();
   }
}


void Server::copyRunFile()
{
   Network::Reply* reply(qobject_cast<Network::Reply*>(sender()));

   if (reply && m_activeRequests.contains(reply)) {
      Job* job(m_activeRequests.value(reply));
      m_activeRequests.remove(reply);

      if (!job) {
         QLOG_WARN() << "Invalid Job pointer";
         //reply->deleteLater();
         return;
      }

      if (reply->status() != Network::Reply::Finished) {
         job->setStatus(Job::Error, reply->message());
         JobMonitor::instance().jobSubmissionFailed(job);
         //reply->deleteLater();
         return;
      }

      //reply->deleteLater();

      QString fileContents(m_configuration.value(ServerConfiguration::RunFileTemplate));
      fileContents = substituteMacros(fileContents);
      fileContents = job->substituteMacros(fileContents);
      fileContents += "\n";
      QString fileName(Util::WriteToTemporaryFile(fileContents));

      qDebug() << "Run file contents written to" << fileName;

      QString destination(job->jobInfo().getRemoteFilePath(QChemJobInfo::RunFileName));
#ifdef Q_OS_WIN32
      if (isLocal()) destination = job->jobInfo().getRemoteFilePath(QChemJobInfo::BatchFileName);
#endif
      reply = m_connection->putFile(fileName, destination);
      connect(reply, SIGNAL(finished()), this, SLOT(queueJob()));
      m_activeRequests.insert(reply, job);
      reply->start();

   }else {
      QLOG_ERROR() << "Server Error: invalid reply";
      //if (reply) reply->deleteLater();
   }
}


void Server::queueJob()
{
   Network::Reply* reply(qobject_cast<Network::Reply*>(sender()));

   if (reply && m_activeRequests.contains(reply)) {
      Job* job(m_activeRequests.value(reply));
      m_activeRequests.remove(reply);
      if (!job) {
         QLOG_WARN() << "Invalid Job pointer";
         //reply->deleteLater();
         return;
      }

      if (reply->status() != Network::Reply::Finished) {
         job->setStatus(Job::Error, reply->message());
         JobMonitor::instance().jobSubmissionFailed(job);
         //reply->deleteLater();
         return;
      }

      //reply->deleteLater();
      QString submit(m_configuration.value(ServerConfiguration::Submit));
      submit = substituteMacros(submit);
      submit = job->substituteMacros(submit);

      QString workingDirectory(job->jobInfo().get(QChemJobInfo::RemoteWorkingDirectory));

      if (isBasic()) {
         // Cache a list of currently running qchem jobs so we can identify the new one
         QString exeName(m_configuration.value(ServerConfiguration::QueueInfo));
         m_qcprogs = System::GetMatchingProcessIds(exeName);
         m_cmds    = System::GetMatchingProcessIds("cmd.exe");
      }

      // Note that the execute takes a working directory which is changed into before
      // executing the command.  If the Working Directory path is relative, and if
      // cd ${JOB_DIR} is in the submit command, this results in the change of directory
      // occuring twice, and if the paths are relative, the path will likely not exist.
      reply = m_connection->execute(submit, workingDirectory);
      connect(reply, SIGNAL(finished()), this, SLOT(submitFinished()));
      m_activeRequests.insert(reply, job);
      reply->start();

   }else {
      QLOG_ERROR() << "Server Error: invalid reply";
      //if (reply) reply->deleteLater();
   }
}


void Server::submitFinished()
{
   Network::Reply* reply(qobject_cast<Network::Reply*>(sender()));

   if (reply && m_activeRequests.contains(reply)) {
      Job* job(m_activeRequests.value(reply));
      m_activeRequests.remove(reply);

      if (job) {
         if (reply->status() == Network::Reply::Finished && 
              parseSubmitMessage(job, reply->message())) {
            job->setStatus(Job::Queued);
            JobMonitor::instance().jobSubmissionSuccessful(job);
            watchJob(job);
         }else {
            job->setStatus(Job::Error, reply->message());
            JobMonitor::instance().jobSubmissionFailed(job);
         }
      }else {
         QLOG_WARN() << "Invalid Job pointer";
      }

      //reply->deleteLater();

   }else {
      QLOG_ERROR() << "Server Error: invalid reply";
   }
}



// This should be delegated
bool Server::parseSubmitMessage(Job* job, QString const& message)
{
   QLOG_TRACE() << "Submit returned message of length:" << message.size();
   QLOG_TRACE() << "Submit returned:\n" << message;

   bool ok(false);

   switch (m_configuration.queueSystem()) {
      case ServerConfiguration::PBS: {
         // A successful submission returns a single token containing the job ID
         QStringList tokens(message.split(QRegularExpression("\\s+"), IQmolSkipEmptyParts));
         if (tokens.size() >= 1) {
            job->setJobId(tokens.first());
            QLOG_DEBUG() << "PBS job submitted with id" << job->jobId();
            ok = true;
         } 
      } break;
         
      case ServerConfiguration::Web: {
         QRegularExpression rx("Qchemserv-Jobid::([0-9a-zA-Z\\-_]+)");
         QRegularExpressionMatch match(rx.match(message));
         if (message.contains("Qchemserv-Status::OK") && match.hasMatch()) {
            job->setJobId(match.captured(1));
            ok = true;
         }
      } break;

      case ServerConfiguration::AWS: {
         QStringList tokens(message.split(QRegularExpression("\\s+"), IQmolSkipEmptyParts));
         if (message.contains("Submitted job id") && tokens.size() == 4) {
            job->setJobId(tokens[3]);
            ok = true;
         }
      } break;

      case ServerConfiguration::SGE: {
         // A successful submission returns a string like:
         //   Your job 2834 ("test.sh") has been submitted
         QStringList tokens(message.split(QRegularExpression("\\s+"), IQmolSkipEmptyParts));
         if (message.contains("has been submitted")) {
            if (tokens.size() >= 3) {
               int id(tokens[2].toInt(&ok));
               if (ok) job->setJobId(QString::number(id));
               ok = true;
            }
         }
      } break;

      case ServerConfiguration::SLURM: {
         // A successful submission returns a string like:
         //   Submitted batch job 1234
         QStringList tokens(message.split(QRegularExpression("\\s+"), IQmolSkipEmptyParts));
         if (message.contains("Submitted batch job")) {
            if (tokens.size() >= 4) {
               int id(tokens[3].toInt(&ok));
               if (ok) job->setJobId(QString::number(id));
               ok = true;
            }
         }
      } break;

      case ServerConfiguration::Basic: {
         if (isLocal()) {

            // First see if we can capture the PID from the return from the 
            //  submission script
            QRegularExpression rx("JobId:\\s+([0-9]+)");
            QRegularExpressionMatch match(rx.match(message));
            if (match.hasMatch()) {
               job->setJobId(match.captured(1));
               ok = true;
               QLOG_DEBUG() << "PID captured from return message" << match.captured(1);
            }

            // No dice, so we search through the qchem processes:
            if (!ok) {
               int count(0);

               QString exeName(m_configuration.value(ServerConfiguration::QueueInfo));

               QList<unsigned> qcprogs = System::GetMatchingProcessIds(exeName);

               while (count <= 4 && qcprogs.size() <= m_qcprogs.size()) {
                  qDebug() << "searching" <<  count << qcprogs.size() << m_qcprogs.size();
                  QThread::msleep(500) ;
                  qcprogs = System::GetMatchingProcessIds(exeName);
                  count++;
               }

               QLOG_DEBUG() << "Original list of PIDS matching:" << exeName;
               QLOG_DEBUG() << m_qcprogs; 
               QLOG_DEBUG() << "New list of PIDS:";
               QLOG_DEBUG() << qcprogs; 
  
               QList<unsigned>::iterator iter;
#ifdef WIN32
               int jobid(-1);
#else
               int jobid(0);
#endif
               for (iter = qcprogs.begin(); iter != qcprogs.end(); ++iter) {
                   if (!m_qcprogs.contains(*iter)) {
                      qDebug() << "Setting job id to:" << QString::number(*iter);
                      jobid = *iter;
                      break;
                   }
               }
               job->setJobId(QString::number(jobid));
               ok = true;
            }

         }else {
            QStringList tokens(message.split(QRegularExpression("\\s+"), IQmolSkipEmptyParts));
            if (tokens.size() == 1) {  // bash returns only the pid
               int id(tokens[0].toInt(&ok));
               if (ok) job->setJobId(QString::number(id));
            }else if (tokens.size() >= 2) { // skip csh initial [1] 
               int id(tokens[1].toInt(&ok));
               if (ok) job->setJobId(QString::number(id));
            }
         }
/*
         qDebug() << "Need to correctly parse submit message for server type "
                  << ServerConfiguration::toString(m_configuration.queueSystem());
         QLOG_DEBUG() << "Submit message:" << message;

         // A successful submission returns a string like:
         //   [1] 9876 $QC/exe/qcprog.exe .aaaa.inp.9876.qcin.1 $QCSCRATCH/local/qchem9876
         // ...or on Windows we parse for the following 
         //   ProcessId = 1234

         if (message.contains("ProcessId =")) { // Windows
            QRegularExpression rx("ProcessId =\\s+(\\d+)\\s+=");
            if (rx.indexIn(message) != -1) {
               int id(rx.cap(1).toInt(&ok));
               if (ok) job->setJobId(QString::number(id));
            }else {
               // It is possible that the job has completed before the batch file
               // determines its PID, so we let this slide.
               ok = true;
            }
         }else {
         }
*/
      } break;

      default:
         qDebug() << "########### Need to parse submit message for server type ###########"
                  << ServerConfiguration::toString(m_configuration.queueSystem());
         break;
   }

   return ok;
}



// ---------- Query ----------
void Server::query(Job* job)
{
   if (!job) {
      QLOG_WARN() << "Query called on invalid job";
      return;
   }

   QList<Network::Reply*> keys(m_activeRequests.keys(job));

   if (!keys.isEmpty()) {
      QLOG_DEBUG() << "Query on busy job";
      return;
   }

   if (!open()) return;

   QString query(m_configuration.value(ServerConfiguration::Query));
   query = substituteMacros(query);
   query = job->substituteMacros(query);

   qDebug() << "Query string:" << query;

   Network::Reply* reply(m_connection->execute(query));
   connect(reply, SIGNAL(finished()), this, SLOT(queryFinished()));
   m_activeRequests.insert(reply, job);
   reply->start();
}


void Server::queryFinished()
{
   Network::Reply* reply(qobject_cast<Network::Reply*>(sender()));

   if (reply && m_activeRequests.contains(reply)) {
      Job* job(m_activeRequests.value(reply));
      m_activeRequests.remove(reply);

      if (job) {
         if (reply->status() != Network::Reply::Finished || 
            !parseQueryMessage(job, reply->message())) {
            job->setStatus(Job::Unknown, reply->message());
          }
      }else {
         QLOG_WARN() << "Invalid Job pointer";
      }

      //reply->deleteLater();

   }else {
      QLOG_ERROR() << "Server Error: invalid query reply";
   }
}


// This should be delegated
bool Server::parseQueryMessage(Job* job, QString const& message)
{  
   bool ok(false);
   Job::Status status(Job::Unknown);
   QString statusMessage;

   switch (m_configuration.queueSystem()) {

      case ServerConfiguration::PBS: {

         if (message.isEmpty()) {
            // assume finished, but need to check for errors
            status = Job::Finished;
            ok = true;
         }else {

            QStringList lines(message.split(QRegularExpression("\\n"), IQmolSkipEmptyParts));
            QStringList tokens;
            QStringList::iterator iter;

            for (iter = lines.begin(); iter != lines.end(); ++iter) {
                //job_state = R
                if ((*iter).contains("job_state =")) {
                   tokens = (*iter).split(QRegularExpression("\\s+"), IQmolSkipEmptyParts);
                   if (tokens.size() >= 3) {
                      if (tokens[2] == "R" || tokens[2] == "E") {
                         status = Job::Running;
                      }else if (tokens[2] == "S" || tokens[2] == "H") {
                         status = Job::Suspended;
                      }else if (tokens[2] == "Q" || tokens[2] == "W") {
                         status = Job::Queued;
                      }else if (tokens[2] == "F") {
                         status = Job::Finished;
                      }
                      ok = true;
                   }
                }

                if ((*iter).contains("resources_used.cput")) {
                   QString time((*iter).split(QRegularExpression("\\s+"), IQmolSkipEmptyParts).last());
                   job->resetTimer(Util::Timer::toSeconds(time));
                }else if ((*iter).contains("comment =")) {
                   statusMessage = (*iter).remove("comment = ").trimmed();
                }
            }
         }
      } break;
         
      case ServerConfiguration::Web: {
         QRegularExpression rx("Qchemserv-Jobstatus::([A-Z]+)");
         QRegularExpressionMatch match(rx.match(message));
         if (message.contains("Qchemserv-Status::OK") && match.hasMatch()) {
            QString rv(match.captured(1));
            if (rv == "DONE")    status = Job::Finished;
            if (rv == "RUNNING") status = Job::Running;
            if (rv == "QUEUED")  status = Job::Queued;
            if (rv == "NEW")     status = Job::Queued;
            if (rv == "ERROR")   status = Job::Error;
            ok = true;
          }
      } break;

      case ServerConfiguration::AWS: {
         if (m_connection) {
            QStringMap map = m_connection->parseQueryMessage(message);
            status = Job::fromString(map["status"]);
            statusMessage = map["message"];
            ok = true;
         }
      } break;

      case ServerConfiguration::SGE: {
         if (message.isEmpty() || message.contains("not exist")) {
            status = Job::Finished;
         }else {
            int nTokens;
            QString input(message);
            QStringList tokens;
            Parser::TextStream textStream(&input);

            while (!textStream.atEnd()) {
               tokens = textStream.nextLineAsTokens();
               nTokens = tokens.size();

               if (nTokens >= 5 && tokens.first().contains(job->jobId())) {
                  QString s(tokens[4]);
                  if (s.contains("q", Qt::CaseSensitive)) {
                     status = Job::Queued;
                  }else if (s.contains("s", Qt::CaseInsensitive)) {
                     status = Job::Suspended;
                  }else if (s.contains("r", Qt::CaseSensitive)) {
                     status = Job::Running;
                  }
                  ok = true;
               }else if ( (nTokens > 1) && 
                  tokens.first().contains("usage"), Qt::CaseInsensitive ) {
                  QRegularExpression rx("cpu=([\\d:]+)");
                  for (int i = 1; i < tokens.size(); ++i) {
                      QRegularExpressionMatch match(rx.match(tokens[i]));
                      if (match.hasMatch()) {
                         qDebug() << "RegExp matched:" << match.captured(1);
                         job->resetTimer(Util::Timer::toSeconds(match.captured(1)));
                      }
                  }
               }
            }
         }
      } break;

      case ServerConfiguration::SLURM: {
         if (message.isEmpty() || message.contains("COMPLETED") ) {
            status = Job::Finished;

         }else if (message.contains("PENDING") || message.contains("REQUEUED")) {
            status = Job::Queued;

         }else if (message.contains("RUNNING")) {
            status = Job::Running;

         }else if (message.contains("SUSPENDED")) {
            status = Job::Suspended;

         }else {
            statusMessage = message;
            status = Job::Error;
         }
      } break;

      case ServerConfiguration::Basic: {
         if (message.isEmpty()) {
            status = Job::Finished;
         }else if (message.contains("No tasks are running")) { // Windows
            status = Job::Finished;
         }else if (message.contains("ERROR")) { // Windows
            status = Job::Finished;
         }else if (message.contains(Preferences::ServerQueryJobFinished())) { // Windows
            status = Job::Finished;
         }else {
            status = Job::Running;
         }
         ok = true;
      } break;


      default:
         QLOG_ERROR() << "Need to parse query message for server type "
                      << ServerConfiguration::toString(m_configuration.queueSystem());
         break;
   }

   // Only print message if there has been a change in status
   if (job->status() != status) {
      QLOG_TRACE() << "Query returned:" << message;
      QLOG_TRACE() << "parseQueryMessage setting status to " << Job::toString(status) << ok;
   }

   if (!Job::isActive(status)) unwatchJob(job);
   job->setStatus(status, statusMessage); 

   return ok;
}



// ---------- kill ----------
void Server::kill(Job* job)
{
   QList<Network::Reply*> keys(m_activeRequests.keys(job));

   if (!job) {
       QLOG_WARN() << "Kill called on invalid job";
       return;
   }

   if (!keys.isEmpty()) {
      QLOG_WARN() << "Kill called on busy job";
      return;
   }

   if (isLocal()) {
      //we currently 
   }

   if (!open()) return;

   QString kill(m_configuration.value(ServerConfiguration::Kill));
   kill = substituteMacros(kill);
   kill = job->substituteMacros(kill);

   qDebug() << "Kill string:" << kill;

   Network::Reply* reply(m_connection->execute(kill));
   connect(reply, SIGNAL(finished()), this, SLOT(killFinished()));
   m_activeRequests.insert(reply, job);
   reply->start();
}


void Server::killFinished()
{
   Network::Reply* reply(qobject_cast<Network::Reply*>(sender()));

   if (reply && m_activeRequests.contains(reply)) {
      Job* job(m_activeRequests.value(reply));
      m_activeRequests.remove(reply);
      if (!job) {
         QLOG_WARN() << "Invalid Job pointer";
         //reply->deleteLater();
         return;
      }

      unwatchJob(job);
      job->setStatus(Job::Killed, reply->message());
      //reply->deleteLater();

   }else {
      QLOG_ERROR() << "Server Error: invalid kill reply";
   }
}



// ---------- Copy ----------
void Server::copyResults(Job* job)
{
   if (isLocal()) return;

   QList<Network::Reply*> keys(m_activeRequests.keys(job));

   if (!job) {
       QLOG_WARN() << "Copy called on invalid job";
       return;
   }

   if (!keys.isEmpty()) {
      QLOG_WARN() << "Copy called on busy job";
      return;
   }

   if (!open()) return;

   QString listCmd(m_configuration.value(ServerConfiguration::JobFileList));
   listCmd = substituteMacros(listCmd);
   listCmd = job->substituteMacros(listCmd);
   qDebug() << "list File command :" << listCmd;

   job->setStatus(Job::Copying);
   Network::Reply* reply(m_connection->execute(listCmd));
   connect(reply, SIGNAL(finished()), this, SLOT(listFinished()));
   m_activeRequests.insert(reply, job);
   reply->start();
}


void Server::listFinished()
{
   Network::Reply* reply(qobject_cast<Network::Reply*>(sender()));

   if (reply && m_activeRequests.contains(reply)) {
      Job* job(m_activeRequests.value(reply));
      m_activeRequests.remove(reply);
      if (!job) {
         QLOG_WARN() << "Invalid Job pointer";
         return;
      }

      if (reply->status() != Network::Reply::Finished) {
         QString s("Copy failed:\n");
         s += reply->message();
         job->setStatus(Job::Error, s);
         return;
      }

      QStringList fileList(parseListMessage(job, reply->message()));
      qDebug() << "File list from server:" << fileList;
      // Remove unwanted files
      fileList.removeAll("batch");
      // This one is for the IQmol server, which can't handle directories.  This is the name
      // of the directory that holds the FSM files.
      unsigned pos(fileList.indexOf(QRegularExpression(".*input.files")));
      if (pos >= 0) fileList.removeAt(pos);
      QString destination(job->jobInfo().get(QChemJobInfo::LocalWorkingDirectory));
      //reply->deleteLater();

      reply = m_connection->getFiles(fileList, destination);
      connect(reply, SIGNAL(copyProgress(double)), job, SLOT(copyProgress(double)));
      connect(reply, SIGNAL(finished()), this, SLOT(copyResultsFinished()));
      m_activeRequests.insert(reply, job);
      reply->start();

   }else {
      QLOG_ERROR() << "Server Error: invalid query reply";
      // if (reply) reply->deleteLater();
   }
}


QStringList Server::parseListMessage(Job* job, QString const& message)
{
   QStringList tokens(message.split("\n"));
   QStringList list;

   for (int i = 0; i < tokens.size(); ++i) {
       QString file(tokens[i]);
       if (!file.isEmpty() && 
           // avoid copying the duplicate individual checkpoint files
           !file.contains(QRegularExpression("input\\.\\d+\\.FChk")) &&
           file != "pathtable") {
           list << file;
       }
   }

   if (m_configuration.connection() == Network::AWS) {
      QString download(m_configuration.value(ServerConfiguration::QueueInfo));
      download = substituteMacros(download);
      download = job->substituteMacros(download);
      list.clear();
      list.append(download);

   }else if (m_configuration.isWebBased()) {
      QString download(m_configuration.value(ServerConfiguration::QueueInfo));
      download = substituteMacros(download);
      download = job->substituteMacros(download);

      for (int i = 0; i < list.size(); ++i) {
          QString tmp(download);
          list[i] = tmp.replace("${FILE_NAME}", list[i]);
      }
   }

   return list;
}


void Server::copyResultsFinished()
{
   Network::Reply* reply(qobject_cast<Network::Reply*>(sender()));

   if (reply && m_activeRequests.contains(reply)) {
      Job* job(m_activeRequests.value(reply));
      m_activeRequests.remove(reply);
      if (!job) {
         QLOG_WARN() << "Invalid Job pointer";
         return;
      }

      if (reply->status() != Network::Reply::Finished) {
         QString s("Copy failed:\n");
         s += reply->message();
         job->setStatus(Job::Error, s);
         return;
      }

      QString msg("Results in: ");
      msg += job->jobInfo().get(QChemJobInfo::LocalWorkingDirectory);
      job->jobInfo().localFilesExist(true);
      job->setStatus(Job::Finished, msg);
      //reply->deleteLater();

   }else {
      QLOG_ERROR() << "Server Error: invalid copy reply";
      //if (reply) reply->deleteLater();
   }
}


void Server::cancelCopy(Job* job)
{
   if (!job) {
      QLOG_WARN() << "Invalid Job pointer";
      return;
   }

   Network::Reply* reply(m_activeRequests.key(job));

   if (reply && m_activeRequests.contains(reply)) {
      m_activeRequests.remove(reply);
      job->setStatus(Job::Error, "Copy canceled");
      reply->interrupt();
      //reply->deleteLater();
   }else {
      QLOG_ERROR() << "Server Error: invalid copy reply";
      //if (reply) reply->deleteLater();
   }
}


// --------------------------

QString Server::substituteMacros(QString const& input)
{
   QString output(input);
   output.remove("POST");
   output.remove("GET");
   output = output.trimmed();
   
   output.replace("${COOKIE}",     m_configuration.value(ServerConfiguration::Cookie));
   output.replace("${USERNAME}",   m_configuration.value(ServerConfiguration::UserName));
   output.replace("${SERVERNAME}", m_configuration.value(ServerConfiguration::ServerName));
   return output;
}


void Server::setUpdateInterval(int const seconds)
{
   m_updateTimer.setInterval(seconds*1000);
}


void Server::unwatchJob(Job* job)
{
   if (m_watchedJobs.contains(job)) m_watchedJobs.removeAll(job); 
   if (m_watchedJobs.isEmpty()) stopUpdates();
}


void Server::watchJob(Job* job)
{
   if (job) {
      if (!m_watchedJobs.contains(job)) {
         m_watchedJobs.append(job); 
         connect(job, SIGNAL(deleted(Job*)), this, SLOT(unwatchJob(Job*)));
      }
      if (m_connection && m_connection->isConnected()) startUpdates();
   }
}

} } // end namespace IQmol::Process
