import { observable } from 'mobx';
import axios from 'axios';
import * as Cookies from 'js-cookie';
import { split } from 'apollo-link';
import { ApolloClient } from 'apollo-client';
import { InMemoryCache } from 'apollo-cache-inmemory';
import { HttpLink } from 'apollo-link-http';
import { onError } from 'apollo-link-error';
import { ApolloLink } from 'apollo-link';
import { WebSocketLink } from 'apollo-link-ws';
import { getMainDefinition } from 'apollo-utilities';
import gql from "graphql-tag";

class AppState {
  @observable timer: number = 0;
  @observable userId: string | null = null;
  @observable errorMessages: string[] = [];
  @observable publicKey: string = "";
  @observable name: string = "Player";
  errorClearTime: number | null = null;
  client: any | null = null;

  static readonly ALERT_SHOW_TIME: number = 5;

  constructor() {
    // Create an http link:
    const httpLink = new HttpLink({
      uri: '/graphql'
    });

    // Create a WebSocket link:
    const wsLink = new WebSocketLink({
      uri: 'ws://' + location.host + `/subscriptions`,
      options: {
        reconnect: true
      }
    });

    // using the ability to split links, you can send data to each link
    // depending on what kind of operation is being sent
    const link = split(
      // split based on operation type
      ({ query }) => {
        const definition = getMainDefinition(query);
        return (
          definition.kind === 'OperationDefinition' &&
          definition.operation === 'subscription'
        );
      },
      wsLink,
      httpLink,
    );

    this.client = new ApolloClient({
      link: ApolloLink.from([
        onError(({ graphQLErrors, networkError }) => {
          if (graphQLErrors)
            graphQLErrors.map(({ message, locations, path }) =>
              console.log(
                `[GraphQL error]: Message: ${message}, Location: ${locations}, Path: ${path}`,
              ),
            );
          if (networkError) console.log(`[Network error]: ${networkError}`);
        }),
        link
      ]),
      cache: new InMemoryCache()
    });

    let outerThis = this;
    let user_id = Cookies.get('user_id');
    if (user_id) {
      this.client.query({
        query: gql`
        {
          me {
            id
            name
            publicKey
            endpoints
          }
        }`
      }).then((result: any) => {
        let data: any = result.data;
        console.log("GOT DATA:");
        console.log(data.me);
        outerThis.userId = data.me.id;
        outerThis.name = data.me.name;
        outerThis.publicKey = data.me.publicKey;
        console.log(outerThis.publicKey);
      })
    }
    setInterval(() => {
      this.timer += 1;
      if (this.errorClearTime !== null && this.timer >= this.errorClearTime) {
        this.errorMessages.shift();
        if (this.errorMessages.length > 0) {
          this.errorClearTime = this.timer + AppState.ALERT_SHOW_TIME;
        } else {
          this.errorClearTime = null;
        }
      }
    }, 1000);
  }

  addErrorMessage(m: string) {
    if (this.errorClearTime === null) {
      this.errorClearTime = this.timer + AppState.ALERT_SHOW_TIME;
    }
    this.errorMessages.push(m);
  }

  login(username: string, password: string) {
    axios.post(`/login`, { username, password })
      .then(res => {
        console.log("GOT LOGIN RESULT");
        console.log(res);
        console.log(res.data);
        let objData = JSON.parse(res.data);
        this.userId = objData.userId;
      }).catch((error: Error) => {
        // handle error
        console.log(error);
        if (error.message.search("status code 401") >= 0) {
          this.addErrorMessage("Wrong username/password.");
        } else {
          this.addErrorMessage("Invalid Login Attempt: " + error);
        }
      });
  }

  loginGithub() {
    console.log("LOGIN GITHUB");
    window.location.href = "/login/github";
  }

  loginDiscord() {
    window.location.href = "/login/discord";
  }

  setPublicKey(publicKey: string) {
    axios.post(`/api/set_user_info`, { publicKey })
      .then(res => {
        this.publicKey = publicKey;
      }).catch((error: Error) => {
        // handle error
        console.log(error);
      });
  }
}

export default AppState;
