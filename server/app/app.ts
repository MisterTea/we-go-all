var createError = require('http-errors');
var express = require('express');
var path = require('path');
var cookieParser = require('cookie-parser');

var indexRouter = require('../routes/index');
var loginRouter = require('../routes/login');
var hostRouter = require('../routes/host');
var joinRouter = require('../routes/join');
var gameRouter = require('../routes/gameinfo');
var graphlHTTP = require('express-graphql');
var schema = require('../model/schema').default;

var passport = require('./passport_handler');
var logger = require('./logger').logger;

var app = express();

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
app.set('views', path.join(__dirname, '../views'));
app.set('view engine', 'pug');

app.use(express.json());
app.use(express.urlencoded({ extended: true }));
app.use(cookieParser());

// Initialize Passport and restore authentication state, if any, from the
// session.
app.use(passport.initialize());
app.use(passport.session());

app.use(express.static(path.join(__dirname, '../public')));
app.use(express.static(path.join(__dirname, '../react_build')));

//app.use(['/', '/index.html'], indexRouter);
app.use('/api', graphlHTTP({
  schema: schema,
  graphiql: true,
}));
app.use('/host', hostRouter);
app.use('/join', joinRouter);
app.use('/game', gameRouter);
app.use('/login', loginRouter);
app.post('/logout',
  function (req: any, res: any) {
    req.logout();
    res.redirect('/');
  });

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

  // render the error page
  res.status(err.status || 500);
  res.render('error');
});

module.exports = app;
