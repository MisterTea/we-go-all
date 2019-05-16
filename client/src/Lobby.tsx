import * as React from 'react';
//import axios from 'axios';
import './App.css';
import { observer } from 'mobx-react';
import AppState from './AppState';
import gql from "graphql-tag";
import { Query } from "react-apollo";
import { Button } from 'react-bootstrap';

const ACTIVE_GAME_QUERY = gql`
  query ActiveGames {
    activeGames {
      id,
      gameName,
      host {
        id,
        name,
      },
      peers {
        id,
        name
      },
    }
  }
`;

@observer
class Lobby extends React.Component<{ appState: AppState }, {}> {

  constructor(props: Readonly<{ appState: AppState }>) {
    super(props);
  }

  public render() {
    return (
      <Query query={ACTIVE_GAME_QUERY}
        pollInterval={1000}>
        {({ loading, error, data }: { loading?: any, error?: any, data: any }) => {
          if (loading) return "Loading...";
          if (error) return `Error! ${error.message}`;
          console.log("DATA");
          console.log(data);

          return (
            <div>
              <table>
                <thead>
                  <tr>
                    <th>GAME ID</th>
                    <th>GAME NAME</th>
                    <th>HOST</th>
                    <th>PEERS</th>
                    <th></th>
                  </tr>
                </thead>
                <tbody>
                  {data.activeGames.map((game: any) => (
                    <tr key={game.id}>
                      <td>
                        {game.id}
                      </td>
                      <td>
                        {game.gameName}
                      </td>
                      <td>
                        {game.host.name}
                      </td>
                      <td>
                        {
                          game.peers.map((peer: any) => (
                            <span key={peer.id}>{peer.name + ", "}</span>
                          ))
                        }
                      </td>
                      <td>
                        <Button variant="secondary" onClick={(e: any) => {
                          this.props.appState.joinGame(game.id);
                        }}>Join</Button>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
              <Button variant="primary" onClick={(e: any) => {
                this.props.appState.hostGame();
              }}>
                Host Game
              </Button>
            </div>
          );
        }}
      </Query>
    );
  }
}

export default Lobby;