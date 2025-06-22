import React, { useState, useMemo } from 'react';
import { Search, Filter, Plus, Trash2, Play, Music2, Calendar, Clock } from 'lucide-react';
import { Song, FilterType, SortType, GroupType, User, MusicLibraryProps } from './types';
import { mockSongs } from './data/mockSongs';
import AddSongModal from './components/AddSongModal';
import './index.css';

const MusicLibrary: React.FC<MusicLibraryProps> = ({ user }) => {
  const [songs, setSongs] = useState<Song[]>(mockSongs);
  const [searchTerm, setSearchTerm] = useState('');
  const [filterType, setFilterType] = useState<FilterType>('all');
  const [sortBy, setSortBy] = useState<SortType>('title');
  const [groupBy, setGroupBy] = useState<GroupType>('none');
  const [showAddModal, setShowAddModal] = useState(false);

  // Filter songs based on search term and filter type
  const filteredSongs = useMemo(() => {
    return songs.filter(song => {
      const searchLower = searchTerm.toLowerCase();
      
      if (filterType === 'all') {
        return song.title.toLowerCase().includes(searchLower) ||
               song.artist.toLowerCase().includes(searchLower) ||
               song.album.toLowerCase().includes(searchLower);
      }
      
      return song[filterType].toLowerCase().includes(searchLower);
    });
  }, [songs, searchTerm, filterType]);

  // Sort filtered songs
  const sortedSongs = useMemo(() => {
    return [...filteredSongs].sort((a, b) => {
      const aValue = a[sortBy];
      const bValue = b[sortBy];
      
      if (typeof aValue === 'string' && typeof bValue === 'string') {
        return aValue.localeCompare(bValue);
      }
      
      return Number(aValue) - Number(bValue);
    });
  }, [filteredSongs, sortBy]);

  // Group sorted songs
  const groupedSongs = useMemo(() => {
    if (groupBy === 'none') {
      return { 'All Songs': sortedSongs };
    }

    return sortedSongs.reduce((groups, song) => {
      const key = song[groupBy];
      if (!groups[key]) {
        groups[key] = [];
      }
      groups[key].push(song);
      return groups;
    }, {} as Record<string, Song[]>);
  }, [sortedSongs, groupBy]);

  const handleAddSong = (newSong: Omit<Song, 'id'>) => {
    const song: Song = {
      ...newSong,
      id: Date.now().toString()
    };
    setSongs(prevSongs => [...prevSongs, song]);
    setShowAddModal(false);
  };

  const handleDeleteSong = (songId: string) => {
    setSongs(prevSongs => prevSongs.filter(song => song.id !== songId));
  };

  // Calculate statistics using reduce
  const stats = useMemo(() => {
    return songs.reduce((acc, song) => {
      acc.totalSongs++;
      acc.artists.add(song.artist);
      acc.albums.add(song.album);
      acc.genres[song.genre] = (acc.genres[song.genre] || 0) + 1;
      return acc;
    }, {
      totalSongs: 0,
      artists: new Set<string>(),
      albums: new Set<string>(),
      genres: {} as Record<string, number>
    });
  }, [songs]);

  return (
    <div className="min-h-screen bg-gradient-to-br from-purple-900 via-blue-900 to-indigo-900">
      <div className="absolute inset-0 bg-black/20"></div>
      
      <div className="relative z-10 max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        {/* Micro Frontend Badge */}
        <div className="mb-6">
          <div className="inline-flex items-center px-3 py-1 bg-gradient-to-r from-green-500/20 to-blue-500/20 border border-green-400/30 rounded-full">
            <div className="w-2 h-2 bg-green-400 rounded-full mr-2 animate-pulse"></div>
            <span className="text-green-300 text-sm font-medium">Music Library Micro Frontend</span>
          </div>
        </div>

        {/* Stats Overview */}
        <div className="grid grid-cols-1 md:grid-cols-4 gap-6 mb-8">
          <div className="bg-white/10 backdrop-blur-sm rounded-xl p-6 border border-white/20">
            <div className="flex items-center">
              <Music2 className="w-8 h-8 text-purple-400" />
              <div className="ml-4">
                <p className="text-white/70 text-sm">Total Songs</p>
                <p className="text-2xl font-bold text-white">{stats.totalSongs}</p>
              </div>
            </div>
          </div>
          
          <div className="bg-white/10 backdrop-blur-sm rounded-xl p-6 border border-white/20">
            <div className="flex items-center">
              <Play className="w-8 h-8 text-blue-400" />
              <div className="ml-4">
                <p className="text-white/70 text-sm">Artists</p>
                <p className="text-2xl font-bold text-white">{stats.artists.size}</p>
              </div>
            </div>
          </div>
          
          <div className="bg-white/10 backdrop-blur-sm rounded-xl p-6 border border-white/20">
            <div className="flex items-center">
              <Calendar className="w-8 h-8 text-green-400" />
              <div className="ml-4">
                <p className="text-white/70 text-sm">Albums</p>
                <p className="text-2xl font-bold text-white">{stats.albums.size}</p>
              </div>
            </div>
          </div>
          
          <div className="bg-white/10 backdrop-blur-sm rounded-xl p-6 border border-white/20">
            <div className="flex items-center">
              <Clock className="w-8 h-8 text-yellow-400" />
              <div className="ml-4">
                <p className="text-white/70 text-sm">Genres</p>
                <p className="text-2xl font-bold text-white">{Object.keys(stats.genres).length}</p>
              </div>
            </div>
          </div>
        </div>

        {/* Controls */}
        <div className="bg-white/10 backdrop-blur-sm rounded-xl p-6 border border-white/20 mb-8">
          <div className="flex flex-col lg:flex-row gap-4 items-center justify-between">
            <div className="flex flex-col sm:flex-row gap-4 flex-1">
              {/* Search */}
              <div className="relative flex-1 max-w-md">
                <Search className="absolute left-3 top-3 w-5 h-5 text-white/50" />
                <input
                  type="text"
                  placeholder="Search music..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  className="w-full pl-10 pr-4 py-2 bg-white/10 border border-white/20 rounded-lg text-white placeholder-white/50 focus:outline-none focus:ring-2 focus:ring-purple-500 focus:border-transparent"
                />
              </div>

              {/* Filter Type */}
              <select
                value={filterType}
                onChange={(e) => setFilterType(e.target.value as FilterType)}
                className="px-4 py-2 bg-white/10 border border-white/20 rounded-lg text-white focus:outline-none focus:ring-2 focus:ring-purple-500"
              >
                <option value="all">All Fields</option>
                <option value="title">Title</option>
                <option value="artist">Artist</option>
                <option value="album">Album</option>
              </select>

              {/* Sort */}
              <select
                value={sortBy}
                onChange={(e) => setSortBy(e.target.value as SortType)}
                className="px-4 py-2 bg-white/10 border border-white/20 rounded-lg text-white focus:outline-none focus:ring-2 focus:ring-purple-500"
              >
                <option value="title">Sort by Title</option>
                <option value="artist">Sort by Artist</option>
                <option value="album">Sort by Album</option>
                <option value="year">Sort by Year</option>
              </select>

              {/* Group */}
              <select
                value={groupBy}
                onChange={(e) => setGroupBy(e.target.value as GroupType)}
                className="px-4 py-2 bg-white/10 border border-white/20 rounded-lg text-white focus:outline-none focus:ring-2 focus:ring-purple-500"
              >
                <option value="none">No Grouping</option>
                <option value="artist">Group by Artist</option>
                <option value="album">Group by Album</option>
                <option value="genre">Group by Genre</option>
              </select>
            </div>

            {/* Add Song Button (Admin Only) */}
            {user?.role === 'admin' && (
              <button
                onClick={() => setShowAddModal(true)}
                className="flex items-center px-4 py-2 bg-gradient-to-r from-purple-500 to-blue-500 text-white rounded-lg hover:from-purple-600 hover:to-blue-600 transition-all duration-200"
              >
                <Plus className="w-5 h-5 mr-2" />
                Add Song
              </button>
            )}
          </div>
        </div>

        {/* Songs Grid */}
        <div className="space-y-8">
          {Object.entries(groupedSongs).map(([groupName, groupSongs]) => (
            <div key={groupName}>
              {groupBy !== 'none' && (
                <h2 className="text-2xl font-bold text-white mb-4 flex items-center">
                  <Filter className="w-6 h-6 mr-2 text-purple-400" />
                  {groupName}
                  <span className="ml-2 text-sm text-white/60 font-normal">
                    ({groupSongs.length} songs)
                  </span>
                </h2>
              )}
              
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-6">
                {groupSongs.map((song) => (
                  <div
                    key={song.id}
                    className="bg-white/10 backdrop-blur-sm rounded-xl p-4 border border-white/20 hover:bg-white/15 transition-all duration-200 group"
                  >
                    <div className="relative mb-4">
                      <img
                        src={song.imageUrl}
                        alt={song.title}
                        className="w-full h-48 object-cover rounded-lg"
                      />
                      <div className="absolute inset-0 bg-black/40 opacity-0 group-hover:opacity-100 transition-opacity duration-200 rounded-lg flex items-center justify-center">
                        <button className="w-12 h-12 bg-white/20 backdrop-blur-sm rounded-full flex items-center justify-center hover:bg-white/30 transition-all duration-200">
                          <Play className="w-6 h-6 text-white ml-1" />
                        </button>
                      </div>
                    </div>
                    
                    <div className="space-y-2">
                      <h3 className="font-semibold text-white truncate">{song.title}</h3>
                      <p className="text-white/70 text-sm truncate">{song.artist}</p>
                      <p className="text-white/60 text-xs truncate">{song.album} • {song.year}</p>
                      <div className="flex items-center justify-between">
                        <span className="text-white/50 text-xs">{song.duration}</span>
                        <div className="flex items-center space-x-2">
                          <span className="px-2 py-1 bg-white/10 rounded-full text-xs text-white/70">
                            {song.genre}
                          </span>
                          {user?.role === 'admin' && (
                            <button
                              onClick={() => handleDeleteSong(song.id)}
                              className="p-1 text-red-400 hover:text-red-300 hover:bg-red-400/20 rounded transition-all duration-200"
                              title="Delete Song"
                            >
                              <Trash2 className="w-4 h-4" />
                            </button>
                          )}
                        </div>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          ))}
        </div>

        {/* No Results */}
        {sortedSongs.length === 0 && (
          <div className="text-center py-12">
            <Music2 className="w-16 h-16 text-white/30 mx-auto mb-4" />
            <h3 className="text-xl font-semibold text-white mb-2">No songs found</h3>
            <p className="text-white/60">Try adjusting your search or filter criteria.</p>
          </div>
        )}
      </div>

      {/* Add Song Modal */}
      {showAddModal && (
        <AddSongModal
          onAdd={handleAddSong}
          onClose={() => setShowAddModal(false)}
        />
      )}
    </div>
  );
};

export default MusicLibrary;