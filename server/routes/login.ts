var express = require('express');
var router = express.Router();
var passport = require("../server/passport_handler");
var config = require('../server/config');

/*
router.post('/',
  passport.authenticate('local'), function (req, res) {
    console.log("AUTH COMPLETE");
    console.log(req);
    console.log(res);
  });
*/

var authCb = function (req, res) {
  console.log("AUTH COMPLETE");
  console.log(req);
  console.log(res);
  console.log(req.user);
  console.log(req.user._id);
  res.cookie("user_id", req.user._id.toString());
  res.redirect('/');
}

let discordAuth = passport.authenticate('discord', { callbackURL: config.login.discord.callbackURL })
router.get('/discord', discordAuth);
router.get(
  '/discord/callback',
  discordAuth,
  authCb);

let githubAuth = passport.authenticate('github', { callbackURL: config.login.github.callbackURL })
router.get('/github', githubAuth);
router.get(
  '/github/callback',
  githubAuth,
  authCb);

module.exports = router;
