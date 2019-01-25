import { exists } from "fs";

var passport = require('passport');
var Strategy = require('passport-local').Strategy;
var GitHubStrategy = require('passport-github').Strategy;
var DiscordStrategy = require('passport-discord').Strategy;
var db = require('../model/db');
var config = require('./config');
var logger = require('./logger').logger;

// Configure the local strategy for use by Passport.
//
// The local strategy require a `verify` function which receives the credentials
// (`username` and `password`) submitted by the user.  The function must verify
// that the password is correct and then invoke `cb` with a user object, which
// will be set at `req.user` in route handlers after authentication.
passport.use(new Strategy(
  function (email: string, password: string, cb: any) {
    console.log("CHECKING PASSWORD: ");
    console.log(email)
    console.log(password);
    db.users.findOne({ email: email }, function (err: any, user: any) {
      console.log("FIND ONE CALLBACK");
      console.log(err);
      console.log(user);
      if (err) { return cb(err); }
      if (!user) { return cb(null, false); }
      if (user.password !== password) { return cb(null, false); }
      return cb(null, user);
    });
  }));

if ("discord" in config.login) {
  logger.info("Setting up discord login");
  passport.use(new DiscordStrategy(config.login.discord,
    function (accessToken, refreshToken, profile, cb) {
      logger.info("GOT PROFILE");
      logger.info(profile);
      const email = profile.email;
      db.users.findOneAndUpdate(
        { email: email },
        { $set: { discordId: profile.id, email: email } },
        { new: true, upsert: true },
        function (err, user) {
          return cb(err, user);
        });
    }));
}

if ("github" in config.login) {
  logger.info("Setting up github login");
  passport.use(new GitHubStrategy(config.login.github,
    function (accessToken, refreshToken, profile, cb) {
      logger.info("GOT PROFILE");
      logger.info(profile);
      const email = profile.emails[0].value;
      db.users.findOneAndUpdate(
        { email: email },
        { $set: { githubId: profile.id, email: email } },
        { new: true, upsert: true },
        function (err, user) {
          return cb(err, user);
        });
    }
  ));
}

// Configure Passport authenticated session persistence.
//
// In order to restore authentication state across HTTP requests, Passport needs
// to serialize users into and deserialize users out of the session.  The
// typical implementation of this is as simple as supplying the user ID when
// serializing, and querying the user record by ID from the database when
// deserializing.
passport.serializeUser(function (user: any, cb: any) {
  cb(null, user.id);
});

passport.deserializeUser(function (id: string, cb: any) {
  db.users.findById(id, function (err: any, user: any) {
    if (err) { return cb(err); }
    cb(null, user);
  });
});

module.exports = {
  "initialize": passport.initialize.bind(passport),
  "session": passport.session.bind(passport),
  "authenticate": passport.authenticate.bind(passport)
}