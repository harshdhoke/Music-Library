import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import MusicLibrary from './MusicLibrary';
import './index.css';

// This is for standalone development
const mockUser = {
  id: '1',
  username: 'admin',
  role: 'admin' as const
};

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <MusicLibrary user={mockUser} />
  </StrictMode>
);