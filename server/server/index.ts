/**
 * Module dependencies.
 */

var logger = require('./logger').logger;
var http = require('http');
var UdpServer = require("./udp_server")
var db = require('../model/db');
import { execute, subscribe } from 'graphql';
import { SubscriptionServer } from 'subscriptions-transport-ws';
var createError = require('http-errors');
var express = require('express');
var path = require('path');
var cookieParser = require('cookie-parser');

var indexRouter = require('../routes/index');
var loginRouter = require('../routes/login');
var apiRouter = require('../routes/api');
import cors from 'cors';
var myGraphQLSchema = require('../model/schema').default;
import { ApolloServer, gql } from 'apollo-server-express';
import bodyParser from 'body-parser';


var passport = require('./passport_handler');
var logger = require('./logger').logger;

var app = express();

/**
 * Get port from environment and store in Express.
 */

var port = normalizePort(process.env.PORT || '3000');
app.set('port', port);
app.use('*', cors({ origin: `http://localhost:${port}` }));

app.use(require('express-bunyan-logger')({
  name: 'logger',
  streams: [{
    level: 'warn',
    stream: process.stdout
  },
  {
    type: 'file',
    path: '/tmp/we-go-all-http.log',
    level: 'info',
  },
  {
    type: 'file',
    path: '/tmp/we-go-all-http-d.log',
    level: 'debug',
  }]
}));

//app.use(require('cookie-parser')());

app.use(require('express-session')({ secret: 'we-go-all', resave: false, saveUninitialized: false }));

// Body parser
//var bodyParser = require('body-parser');
//app.use(bodyParser.json()); // support json encoded bodies
//app.use(bodyParser.urlencoded({ extended: true })); // support encoded bodies

// view engine setup
app.set('views', './views');
app.set('view engine', 'pug');

app.use(express.json());
app.use(express.urlencoded({ extended: true }));
app.use(cookieParser());

// Initialize Passport and restore authentication state, if any, from the
// session.
app.use(passport.initialize());
app.use(passport.session());

//app.use(['/', '/index.html'], indexRouter);

const fs = require('fs');

const typeDefs = fs.readFileSync("model/schema.gql", "utf-8")
import { resolvers } from '../model/resolvers';

const apollo = new ApolloServer({
  // These will be defined for both new or existing servers
  typeDefs,
  resolvers,
  subscriptions: {
    path: '/subscriptions',
    onConnect: () => console.log('Connected to websocket'),
  },
  playground: {
    endpoint: '/graphql',
    subscriptionEndpoint: '/subscriptions',
  }
});

apollo.applyMiddleware({ app });

app.use('/api', apiRouter);
app.use('/login', loginRouter);
app.post('/logout',
  function (req: any, res: any) {
    req.logout();
    res.redirect('/');
  });

let currentApp = app;

/**
 * Create HTTP server.
 */

var server = http.createServer(app);
var udpServer = UdpServer.start();

apollo.installSubscriptionHandlers(server)

//app.use(express.static('./public'));
app.use(express.static('./react_build'));

// catch 404 and forward to error handler
app.use(function (req: any, res: any, next: any) {
  next(createError(404));
});

// error handler
app.use(function (err: any, req: any, res: any, next: any) {
  // set locals, only providing error in development
  res.locals.message = err.message;
  res.locals.error = req.app.get('env') === 'development' ? err : {};

  logger.error("Got 500 Error: " + err);
  //logger.error(path.join(__dirname, '../views'));

  // render the error page
  res.status(err.status || 500);
  res.render('error');
});

/**
 * Listen on provided port, on all network interfaces.
 */

db.start(function (error) {
  if (error) {
    logger.fatal("Error starting database: " + error);
  }
  console.log("Database started");
  server.listen(port, err => {
    if (err) {
      throw new Error(err);
    }
    console.log(`🚀 Server ready at http://localhost:${port}${apollo.graphqlPath}`)
    console.log(`🚀 Subscriptions ready at ws://localhost:${port}${apollo.subscriptionsPath}`)
    /*
    new SubscriptionServer({
      execute,
      subscribe,
      schema: myGraphQLSchema,
    }, {
        server: server,
        path: '/subscriptions',
      });
      */
    console.log('Server loaded');
  });
  server.on('error', onError);
  server.on('listening', onListening);
});

/**
 * Normalize a port into a number, string, or false.
 */

function normalizePort(val) {
  var port = parseInt(val, 10);

  if (isNaN(port)) {
    // named pipe
    return val;
  }

  if (port >= 0) {
    // port number
    return port;
  }

  return false;
}

/**
 * Event listener for HTTP server "error" event.
 */

function onError(error) {
  if (error.syscall !== 'listen') {
    throw error;
  }

  var bind = typeof port === 'string'
    ? 'Pipe ' + port
    : 'Port ' + port;

  // handle specific listen errors with friendly messages
  switch (error.code) {
    case 'EACCES':
      console.error(bind + ' requires elevated privileges');
      process.exit(1);
      break;
    case 'EADDRINUSE':
      console.error(bind + ' is already in use');
      process.exit(1);
      break;
    default:
      throw error;
  }
}

/**
 * Event listener for HTTP server "listening" event.
 */

if (module.hot) {
  module.hot.accept();

  // Next callback is essential: After code changes were accepted     we need to restart the app. server.close() is here Express.JS-specific and can differ in other frameworks. The idea is that you should shut down your app here. Data/state saving between shutdown and new start is possible
  module.hot.dispose(() => {
    udpServer.close();
    server.close();
  });
}

function onListening() {
  var addr = server.address();
  var bind = typeof addr === 'string'
    ? 'pipe ' + addr
    : 'port ' + addr.port;
  logger.info('Listening on ' + bind);
}
