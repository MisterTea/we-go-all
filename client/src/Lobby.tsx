import * as React from 'react';
//import axios from 'axios';
import './App.css';
import { observer } from 'mobx-react';
import AppState from './AppState';

@observer
class Lobby extends React.Component<{ appState: AppState }, {}> {

  constructor(props: Readonly<{ appState: AppState }>) {
    super(props);
  }

  public render() {
    return (
      <div className="Lobby">
        Lobby
      </div>
    );
  }
}

export default Lobby;