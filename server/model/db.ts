var mongoose = require('mongoose');
var config = require('../server/config');
var ObjectId = mongoose.ObjectId;

function requiresString(s: any) {
  return typeof s === 'string' ? false : true
}

var UserSchema = new mongoose.Schema({
  email: {
    type: String,
    index: true,
    unique: true,
    required: requiresString,
  },
  discordId: {
    type: String,
    index: {
      unique: true,
      partialFilterExpression: { discordId: { $type: 'string' } },
    },
  },
  githubId: {
    type: String,
    index: {
      unique: true,
      partialFilterExpression: { githubId: { $type: 'string' } },
    },
  },
  name: {
    type: String,
    index: {
      partialFilterExpression: { name: { $type: 'string' } },
    },
  },
  password: String,
  publicKey: {
    type: String,
    index: {
      partialFilterExpression: { publicKey: { $type: 'string' } },
    },
  },
  endpoints: [String],
});
exports.users = mongoose.model('User', UserSchema);

var GameSchema = new mongoose.Schema({
  gameName: {
    type: String,
    index: {
      partialFilterExpression: { gameName: { $type: 'string' } },
    },
  },
  host: ObjectId,
  peers: [ObjectId],
  ready: [ObjectId],
  active: Boolean,
});
exports.games = mongoose.model('Game', GameSchema);

exports.start = function (callback) {
  console.log("Connecting to database");
  mongoose.connect(config.db.connectString, { useNewUrlParser: true }, callback);
};

exports.errorHandler = function (callback) {
  return function (err, retval) {
    var args = Array.from(arguments);
    if (args[0]) {
      console.error(err);
      return;
    }
    callback(retval);
  };
}

exports.errorHandlerWithResponse = function (res, callback) {
  return function (err, retval) {
    var args = Array.from(arguments);
    if (args[0]) {
      console.error(err);
      res.status(500);
      res.send('Server error');
      return;
    }
    callback(retval);
  };
}