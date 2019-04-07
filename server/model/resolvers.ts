var db = require('./db');
var ObjectId = require('mongoose').Types.ObjectId;

export const resolvers = {
  User: {
  },
  Game: {
    async peers(game, _args, _context: any, _info: any) {
      var peers: any[] = [];
      for (var peerId in game.peers) {
        peers.push(await db.users.findOneById(peerId));
      }
      return peers;
    },
    async host(game, _args, _context: any, _info: any) {
      return await db.users.findOneById(game.host);
    },
  },
  Query: {
    activeGames: async () => {
      return await db.games.find({ active: true });
    },
    getUser: async (_: any, { id }, _context: any, _info: any) => {
      console.log("GETTING USER WITH ID: " + id);
      return await db.users.findById(ObjectId(id));
    },
    me: async (_none: any, _args: any, req: any, _info: any) => {
      if (req.cookies["user_id"]) {
        let userId = req.cookies["user_id"];
        return await db.users.findById(ObjectId(userId));
      } else {
        throw new Error('Not logged in');
      }
    }
  }
};
