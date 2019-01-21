import {
  makeExecutableSchema
} from 'graphql-tools';
import {
  resolvers
} from './resolvers';
import { importSchema } from 'graphql-import';
const fs = require('fs');

const typeDefs = fs.readFileSync("model/schema.gql", "utf-8")
const schema = makeExecutableSchema({
  typeDefs,
  resolvers
});
export default schema;