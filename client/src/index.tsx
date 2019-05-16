import * as React from 'react';
import * as ReactDOM from 'react-dom';
import App from './App';
//import registerServiceWorker from './registerServiceWorker';
import AppState from './AppState';

import 'bootstrap/dist/css/bootstrap.css';
import 'bootstrap-social/bootstrap-social.css';
import './index.css';
import './App.css';

let appState = new AppState(() => {
  ReactDOM.render(
    <App appState={appState} />,
    document.getElementById('root') as HTMLElement
  );
  //registerServiceWorker();
});
