var express = require('express');
var router = express.Router();
var passport = require("../app/passport_handler");

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
  res.redirect('/');
}

router.get('/discord', passport.authenticate('discord'));
router.get(
  '/discord/callback',
  passport.authenticate('discord'),
  authCb);

router.get('/github', passport.authenticate('github'));
router.get(
  '/github/callback',
  passport.authenticate('github'),
  authCb);

module.exports = router;
