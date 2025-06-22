export interface Song {
  id: string;
  title: string;
  artist: string;
  album: string;
  duration: string;
  year: number;
  genre: string;
  imageUrl: string;
}

export interface User {
  id: string;
  username: string;
  role: 'admin' | 'user';
}

export interface MusicLibraryProps {
  user: User | null;
}

export type FilterType = 'all' | 'title' | 'artist' | 'album';
export type SortType = 'title' | 'artist' | 'album' | 'year';
export type GroupType = 'none' | 'artist' | 'album' | 'genre';