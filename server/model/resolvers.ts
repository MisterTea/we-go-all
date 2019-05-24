var db = require('./db');
var ObjectId = require('mongoose').Types.ObjectId;
import { PubSub } from 'graphql-subscriptions';

export const pubsub = new PubSub();
export const resolvers = {
  User: {
  },
  Game: {
    async peers(game, _args, _context: any, _info: any) {
      console.log("GETTING PEERS");
      console.log(game.peers);
      console.log(game);
      var peers: any[] = [];
      for (let i = 0; i < game.peers.length; i++) {
        let peerId = game.peers[i];
        console.log("Getting peer: " + peerId);
        peers.push(await db.users.findById(peerId));
      }
      return peers;
    },
    async host(game, _args, _context: any, _info: any) {
      console.log("GETTING HOST: " + game.host);
      return await db.users.findById(game.host);
    },
  },
  Query: {
    activeGames: async () => {
      return await db.games.find({ active: true });
    },
    getUser: async (_parent: any, { id }, _context: any, _info: any) => {
      console.log("GETTING USER WITH ID: " + id);
      return await db.users.findById(ObjectId(id));
    },
    me: async (_parent: any, _args: any, context: any, _info: any) => {
      let req = context.req;
      console.log("REQUEST");
      console.log(req);
      if (req.cookies["user_id"]) {
        let userId = req.cookies["user_id"];
        return await db.users.findById(ObjectId(userId));
      } else {
        throw new Error('Not logged in');
      }
    }
  },
  Subscription: {
    gameUpdated: {
      subscribe: () => pubsub.asyncIterator('gameUpdated')
    }
  },
  Mutation: {
    updateMe: async (_parent: any, { publicKey, name }, context) => {
      let req = context.req;
      console.log("REQUEST");
      console.log(req);
      if (req.cookies["user_id"]) {
        let userId = req.cookies["user_id"];
        let updates = {};
        if (publicKey !== undefined) {
          updates["publicKey"] = publicKey;
        }
        if (name !== undefined) {
          updates["name"] = name;
        }
        return await db.users.findByIdAndUpdate(ObjectId(userId), { $set: updates });
      } else {
        throw new Error('Not logged in');
      }
    },
    hostGame: async (_parent: any, _params: any, context: any) => {
      let req = context.req;
      console.log("REQUEST");
      console.log(req);
      if (req.cookies["user_id"]) {
        // TODO: Mark existing games by this host as inactive
        let userId = req.cookies["user_id"];
        var game = new db.games({
          gameName: null,
          host: ObjectId(userId),
          createTime: (new Date).getTime(),
          active: true,
          peers: [ObjectId(userId)],
          ready: [],
        });
        return await game.save();
      } else {
        throw new Error('Not logged in');
      }
    },
    joinGame: async (_parent: any, { gameId }: { gameId: string }, context: any) => {
      let gameObjectId = ObjectId(gameId);
      let req = context.req;
      console.log("REQUEST");
      console.log(gameObjectId);
      if (req.cookies["user_id"]) {
        let userId = ObjectId(req.cookies["user_id"]);
        var game = await db.games.findById(gameObjectId);
        console.log("FOUND GAME");
        console.log(game);
        if (game.peers.find((peerId, _index, _array) => { return ObjectId(peerId).equals(userId); })) {
          throw new Error("Tried to join a game that you are already in");
        } else {
          game.peers.push(userId);
          return await game.save();
        }
      } else {
        throw new Error('Not logged in');
      }
    },
  }
};

setInterval(() => {
  pubsub.publish('gameUpdated', { gameUpdated: { id: "123", gameName: "foobar" } });
}, 1000);
